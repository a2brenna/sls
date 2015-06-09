#ifndef __SERIALIZE_H__
#define __SERIALIZE_H__

#include <vector>
#include <string>

/* DESCRIPTION DISK/WIRE FORMAT */
/*
 * Each file or datastream shall contain 1 or more Records in order of ascending value T.
 * Each Record shall contain a uint64_t (T) representing milliseconds since Epoch, followed by
 * A uint64_t (L) representing the length in bytes of the following Datagram, followed by
 * L bytes representing the Data associated with time T.
 *
 * File := Record [Record]
 * Record := T L Datagram
 * T := uint64_t (milliseconds since Epoch)
 * L := uint64_t (length of Datagram
 * Datagram := [Char]
 */

std::vector<std::pair<uint64_t, std::string>> de_serialize(const std::string &raw);

#endif
