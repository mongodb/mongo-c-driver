#!/usr/bin/env python3
"""
setfle2parameter.py modifies and prints an orchestration config file to add the `--setParameter featureFlagFLE2ProtocolVersion2=1` option to mongod and mongos.
Usage: setfle2parameter.py <orchestration config file path>
This file is a temporary workaround until featureFlagFLE2ProtocolVersion2 is enabled by default in SERVER-69563. Once latest server builds have SERVER-69563, this file may be removed.
DRIVERS-2590 tracks removal of this file.
"""

import json
import sys
import unittest
import os


def do_rewrite(config):
    did_rewrite = False

    def rewrite_server(server):
        if "procParams" in server:
            if "setParameter" in server["procParams"]:
                server["procParams"]["setParameter"]["featureFlagFLE2ProtocolVersion2"] = "1"
            else:
                server["procParams"]["setParameter"] = {
                    "featureFlagFLE2ProtocolVersion2": "1"
                }
            return True
        return False

    # Rewrite for a server.
    if rewrite_server(config):
        did_rewrite = True
    # Rewrite for each member in a replica set.
    if "members" in config:
        for server in config["members"]:
            if rewrite_server(server):
                did_rewrite = True
    # Rewrite each shard.
    if "shards" in config:
        for shard in config["shards"]:
            if "shardParams" in shard:
                if "members" in shard["shardParams"]:
                    for server in shard["shardParams"]["members"]:
                        if rewrite_server (server):
                            did_rewrite = True
    # Rewrite each router.
    if "routers" in config:
        for router in config["routers"]:
            # routers do not use `procParams`. Use setParameter directly.
            if "setParameter" in router:
                router["setParameter"]["featureFlagFLE2ProtocolVersion2"] = "1"
            else:
                router["setParameter"] = {
                    "featureFlagFLE2ProtocolVersion2": "1"
                }
            did_rewrite = True


    if not did_rewrite:
        raise Exception(
            "Did not add setParameter. Does the orchestration config have `procParams`?"
        )
    pass


class TestRewrite(unittest.TestCase):
    def test_rewrite(self):
        # Test that setParameter is added for a server.
        input = {
            "procParams": {}
        }
        do_rewrite (input)
        self.assertEqual (input, {
            "procParams": {
                "setParameter": {
                    "featureFlagFLE2ProtocolVersion2": "1"
                }
            }
        })

        # Test that other setParameter values are kept for a server.
        input = {
            "procParams": {
                "setParameter": {
                    "foo": "bar"
                }
            }
        }
        do_rewrite (input)
        self.assertEqual (input, {
            "procParams": {
                "setParameter": {
                    "foo": "bar",
                    "featureFlagFLE2ProtocolVersion2": "1"
                }
            }
        })

        # Test that setParameter is added for a replica_set.
        input = {
            "members": [
                {"procParams": {}},
                {"procParams": {}},
            ]
        }
        do_rewrite(input)
        self.assertEqual(
            input,
            {
                "members": [
                    {
                        "procParams": {
                            "setParameter": {"featureFlagFLE2ProtocolVersion2": "1"}
                        }
                    },
                    {
                        "procParams": {
                            "setParameter": {"featureFlagFLE2ProtocolVersion2": "1"}
                        }
                    },
                ]
            },
        )

        # Test that setParameter is added for shards and routers.
        input = {
            "shards": [
                {"shardParams": {"members": [{"procParams": {}}, {"procParams": {}}]}}
            ],
            "routers": [{}, {}],
        }

        do_rewrite(input)
        self.assertEqual(
            input,
            {
                "shards": [
                    {
                        "shardParams": {
                            "members": [
                                {
                                    "procParams": {
                                        "setParameter": {
                                            "featureFlagFLE2ProtocolVersion2": "1"
                                        }
                                    }
                                },
                                {
                                    "procParams": {
                                        "setParameter": {
                                            "featureFlagFLE2ProtocolVersion2": "1"
                                        }
                                    }
                                },
                            ]
                        }
                    }
                ],
                "routers": [
                    {"setParameter": {"featureFlagFLE2ProtocolVersion2": "1"}},
                    {"setParameter": {"featureFlagFLE2ProtocolVersion2": "1"}},
                ],
            },
        )


if __name__ == "__main__":
    if os.environ.get("SELFTEST", "OFF") == "ON":
        print("Doing self test")
        unittest.main()
        sys.exit(0)

    if len(sys.argv) != 2:
        print(
            "Error: expected path to orchestration config file path as first argument"
        )
        print("Usage: setfle2parameter.py <orchestration config file path>")
        sys.exit(1)

    path = sys.argv[1]
    with open(path, "r") as file:
        config = json.loads(file.read())
    do_rewrite(config)
    print (json.dumps(config, indent=4))

