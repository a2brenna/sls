#!/usr/bin/python

import os, sls_pb2, os.path
ROOT = "/pool/sls/"

keys = os.listdir(ROOT)

for key in keys:
    error_count = 0
    time_index = 0
    num_files = 0
    linked_files = []
    key_dir = ROOT + key + "/"
    files = os.listdir(key_dir)
    if ( files.count("head") != 1): #Do a better check here using stat
        print("Error: " + key + " is missing head!")
        error_count = error_count + 1
        continue
    else:
        files.remove("head")
        num_files = len(files)
        old_file = ""
        next_file = os.path.realpath(key_dir + "head")
        linked_files.append(os.readlink(key_dir + "head"))
        while True:
            archive = sls_pb2.Archive()
            try:
                f = open(next_file, 'r')
            except:
                print("Error: Could not open file " + next_file)
                error_count = error_count + 1
                break

            try:
                archive.ParseFromString(f.read())
            except:
                print("Error: Could not parse file " + next_file)
                error_count = error_count + 1
                break
            finally:
                f.close()

            for v in archive.values:
                if (v.time > time_index):
                    print("Error: " + str(key) + " has misordered data in: " + str(next_file) + " @ " + str(v.time))
                    error_count = error_count + 1
                time_index = v.time

            if(archive.HasField("next_archive")):
                old_file = next_file
                try:
                    next_file = os.path.realpath(key_dir + archive.next_archive)
                except:
                    print("Error: Cannot find next_archive " + archive.next_archive)
                    error_count = error_count + 1
                    break

                linked_files.append(archive.next_archive)
            else:
                break

    orphans = set(files) - set(linked_files)
    orphan_string = " "
    if (len(orphans) > 0):
        for o in (orphans):
            orphan_string = orphan_string + o + " "
        print("Error: " + key + " has " + str(len(orphans)) + " orphan(s):" + orphan_string)
        error_count = error_count + 1

    if( error_count == 0 ):
        print("Key: " + str(key) + " clean")
    else:
        print("Key: " + str(key) + " has " + str(error_count) + " errors")
