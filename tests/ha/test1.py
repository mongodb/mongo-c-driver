#!/usr/bin/env python

import os
import random
import shutil
import signal
import subprocess
import sys
import time

class ReplicaSet:
    name = None
    nodes = None
    port = 0

    def __init__(self, name):
        self.name = name or 'myReplSet'
        self.nodes = []
        self.port = random.randint(30000, 40000)

    def _addNode(self, node):
        if node not in self.nodes:
            node.port, self.port = self.port, self.port + 1
            node.replSet = self.name
            self.nodes.append(node)

    def addReplica(self, replica):
        assert isinstance(replica, Replica)
        self._addNode(replica)

    def addArbiter(self, arbiter):
        assert isinstance(arbiter, Arbiter)
        self._addNode(arbiter)

    def start(self):
        for node in self.nodes:
            node.setup()
            node.start()

        primary = None
        for node in self.nodes:
            if isinstance(node, Replica):
                primary = node
                break

        if not primary:
            assert False, 'No possible primary found!'

        idseq = 0

        config = {'_id': self.name, 'members': []}
        for node in self.nodes:
            config['members'].append({
                '_id': idseq,
                'host': '127.0.0.1:%d' % node.port,
                'arbiterOnly': isinstance(node, Arbiter),
            })
            idseq += 1

        conn = primary.connect()
        command = {'replSetInitiate': config}
        ret = conn.admin.command(command)

        print ret

        # TODO: Connect to first node.
        #       Build configuration for topology.
        #       rs.init(config).

    def kill(self):
        for node in self.nodes:
            try:
                node.kill()
            except Exception, ex:
                print repr(ex)

    def waitForHealthy(self):
        foundPrimary = False
        isHealthy = False
        while not foundPrimary or not isHealthy:
            isHealthy = True
            status = None
            while not status:
                for node in self.nodes:
                    try:
                        status = node.status()
                        if 'members' in status:
                            break
                    except Exception, ex:
                        print repr(ex)
                if not status:
                    print >> sys.stderr, "waitForHealthy(): sleeping 1 second."
                    time.sleep(1)

            for member in status['members']:
                if member['state'] == 1:
                    foundPrimary = True

                if member['stateStr'] not in ('PRIMARY', 'SECONDARY', 'ARBITER'):
                    isHealthy = False

            if not foundPrimary:
                print >> sys.stderr, "waitForHealthy(): sleeping 1 second (no primary)."
                time.sleep(1)

            if not isHealthy:
                print >> sys.stderr, "waitForHealthy(): sleeping 1 second (not healthy)."
                time.sleep(1)

        return True

class Node:
    dbpath = None
    name = None
    port = 0
    proc = None
    replSet = None

    def __init__(self, name, *args, **kwargs):
        self.name = name
        self.dbpath = kwargs.get('dbpath', '.')

    def setup(self):
        if self.dbpath != '.' and os.path.exists(self.dbpath):
            shutil.rmtree(self.dbpath)
        os.makedirs(self.dbpath)

    def connect(self):
        import pymongo
        connStr = 'mongodb://127.0.0.1:%s/' % self.port
        while True:
            try:
                return pymongo.Connection(connStr)
            except:
                print 'Connect failed, waiting 1 second.'
                time.sleep(1)

    def status(self):
        # TODO: This really should not be done by relying on pymongo.
        #       However, using the mongo shell does not currently allow
        #       redirecting input. So while annoying, this works.
        conn = self.connect()
        return conn.admin.command('replSetGetStatus')

    @property
    def argv(self):
        return ['mongod',
                '--nojournal',
                '--noprealloc',
                '--nohttpinterface',
                '--smallfiles',
                '--port', str(self.port),
                '--dbpath', '.',
                '--replSet', self.replSet]

    def start(self):
        self.proc = subprocess.Popen(self.argv,
                                     close_fds=True,
                                     shell=False,
                                     cwd=self.dbpath)

    def kill(self):
        if self.proc:
            self.proc.kill()
            self.proc.wait()
            self.proc = None

class Replica(Node):
    def setup(self):
        Node.setup(self)

class Arbiter(Node):
    def setup(self):
        Node.setup(self)

def test1():
    # Build and start our ReplicaSet
    replSet = ReplicaSet('test1')
    replSet.addReplica(Replica('replica1', dbpath='test1/replica1'))
    replSet.addReplica(Replica('replica2', dbpath='test1/replica2'))
    replSet.addReplica(Replica('replica3', dbpath='test1/replica3'))
    replSet.addArbiter(Arbiter('arbiter1', dbpath='test1/arbiter1'))

    try:
        replSet.start()

        # Wait for the ReplicaSet to become healthy.
        print >> sys.stderr, 'Checking ReplicaSet state.'
        replSet.waitForHealthy()
        print >> sys.stderr, 'ReplicaSet is healthy.'
    finally:
        replSet.kill()

if __name__ == '__main__':
    test1()
