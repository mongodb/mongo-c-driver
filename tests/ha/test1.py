#!/usr/bin/env python

import os
import random
import shutil
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

        # TODO: Connect to first node.
        #       Build configuration for topology.
        #       rs.init(config).

    def waitForHealthy(self):
        notHealthy = True
        while notHealthy:
            notHealthy = False
            for node in self.nodes:
                if node.status() != 1:
                    notHealthy = True
            if notHealthy:
                print >> sys.stderr, "waitForHealthy(): sleeping 1 second."
                time.sleep(1)
        return True

class Node:
    dbpath = None
    name = None
    port = 0
    replSet = None

    def __init__(self, name, *args, **kwargs):
        self.name = name
        self.dbpath = kwargs.get('dbpath', '.')

    def setup(self):
        if self.dbpath != '.' and os.path.exists(self.dbpath):
            shutil.rmtree(self.dbpath)
        os.makedirs(self.dbpath)

    def status(self):
        # TODO: This really should not be done by relying on pymongo.
        #       However, using the mongo shell does not currently allow
        #       redirecting input. So while annoying, this works.
        import pymongo
        connStr = 'mongodb://127.0.0.1:%s/' % 27017 #self.port
        conn = pymongo.Connection(connStr)
        status = conn.admin.command('replSetGetStatus')
        return int(status['myState'])

    @property
    def argv(self):
        return ['mongod',
                '--nojournal',
                '--noprealloc',
                '--nohttpinterface',
                '--smallfiles',
                '--port', str(self.port),
                '--dbpath', self.dbpath,
                '--replSet', self.replSet]

    def start(self):
        print self.argv
        #(stdin, stdout) = os.popen2(self.argv)
        pass

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
    replSet.start()

    # Wait for the ReplicaSet to become healthy.
    print >> sys.stderr, 'Checking ReplicaSet state.'
    replSet.waitForHealthy()
    print >> sys.stderr, 'ReplicaSet is healthy.'

if __name__ == '__main__':
    test1()
