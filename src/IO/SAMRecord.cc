/** @file SAMRecord.cc
 *  @brief This file contains the implementation of the SAMRecord class.
 *  @author Jan Voges (voges)
 *  @bug No known bugs
 */

/*
 *  Changelog
 *  YYYY-MM-DD: What (who)
 */

#include "SAMRecord.h"
#include "Common/Exceptions.h"

SAMRecord::SAMRecord(char *fields[SAMRecord::NUM_FIELDS])
    : qname(fields[0])
    , flag((uint16_t)atoi(fields[1]))
    , rname(fields[2])
    , pos((uint32_t)atoi(fields[3]))
    , mapq((uint8_t)atoi(fields[4]))
    , cigar(fields[5])
    , rnext(fields[6])
    , pnext((uint32_t)atoi(fields[7]))
    , tlen((int64_t)atoi(fields[8]))
    , seq(fields[9])
    , qual(fields[10])
    , opt(fields[11])
    , posMin(0)
    , posMax(0)
    , mapped(false)
{
    check();

    // Compute 0-based first position and 0-based last position this record is
    // to on the reference used for alignment
    posMin = pos - 1;
    posMax = pos - 1;

    size_t cigarIdx = 0;
    size_t cigarLen = cigar.length();
    uint32_t opLen = 0; // length of current CIGAR operation

    for (cigarIdx = 0; cigarIdx < cigarLen; cigarIdx++) {
        if (isdigit(cigar[cigarIdx])) {
            opLen = opLen*10 + (uint32_t)cigar[cigarIdx] - (uint32_t)'0';
            continue;
        }
        switch (cigar[cigarIdx]) {
        case 'M':
        case '=':
        case 'X':
            posMax += opLen;
            break;
        case 'I':
        case 'S':
            break;
        case 'D':
        case 'N':
            posMax += opLen;
            break;
        case 'H':
        case 'P':
            break; // these have been clipped
        default:
            throwErrorException("Bad CIGAR string");
        }
        opLen = 0;
    }

    posMax -= 1;
}

SAMRecord::~SAMRecord(void)
{
    // empty
}

bool SAMRecord::isMapped(void) const
{
    return mapped;
}

void SAMRecord::print(void) const
{
    printf("qname:    %s\n", qname.c_str());
    printf("flag:     %d\n", flag);
    printf("rname:    %s\n", rname.c_str());
    printf("pos:      %d\n", pos);
    printf("mapq:     %d\n", mapq);
    printf("cigar:    %s\n", cigar.c_str());
    printf("rnext:    %s\n", rnext.c_str());
    printf("pnext:    %d\n", pnext);
    printf("tlen:     %d\n", tlen);
    printf("seq:      %s\n", seq.c_str());
    printf("qual:     %s\n", qual.c_str());
    printf("opt:      %s\n", opt.c_str());
    printf("--\n");
    printf("isMapped: %d\n", mapped);
    printf("posMin:   %d\n", posMin);
    printf("posMax:   %d\n", posMax);
}

void SAMRecord::check(void)
{
    // Check all fields
    if (qname.empty() == true) { throwErrorException("qname is empty"); }
    // flag
    if (rname.empty() == true) { throwErrorException("rname is empty"); }
    // pos
    // mapq
    if (cigar.empty() == true) { throwErrorException("cigar is empty"); }
    if (rnext.empty() == true) { throwErrorException("rnext is empty"); }
    // pnext
    // tlen
    if (seq.empty() == true) { throwErrorException("seq is empty"); }
    if (qual.empty() == true) { throwErrorException("qual is empty"); }
    if (opt.empty() == true) { LOG("opt is empty"); }

    // Check if this record is mapped
    if (flag & 0x4 != 0) {
        mapped = false;
    } else {
        mapped = true;
        if (rname == "*" || pos == 0 || cigar == "*" || seq == "*" || qual == "*") {
            throwErrorException("Corrupted record");
        }
    }
}
