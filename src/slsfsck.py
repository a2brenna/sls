#!/usr/bin/python

import os, sls_pb2, os.path
ROOT = "/pool/sls/"

keys = os.listdir(ROOT)

for key in keys:
    real_head = ""
    num_files = 0
    linked_files = []
    key_dir = ROOT + key + "/"
    files = os.listdir(key_dir)
    if ( files.count("head") != 1): #Do a better check here using stat
        print("Error: " + key + " is missing head!")
        continue
    else:
        files.remove("head")
        num_files = len(files)
        next_file = os.path.realpath(key_dir + "head")
        linked_files.append(os.readlink(key_dir + "head"))
        while True:
            try:
                archive = sls_pb2.Archive()
                f = open(next_file, 'r')
                archive.ParseFromString(f.read())
                f.close()
                if(archive.HasField("next_archive")):
                    next_file = os.path.realpath(key_dir + archive.next_archive)
                    linked_files.append(archive.next_archive)
                else:
                    break
            except:
                print("Error: " + key + " has a malformed archive " + next_file)
                raise
                continue

    orphans = set(files) - set(linked_files)
    orphan_string = " "
    if (len(orphans) > 0):
        for o in (orphans):
            orphan_string = orphan_string + o + " "
        print("Error: " + key + " has " + str(len(orphans)) + " orphan(s):" + orphan_string)


