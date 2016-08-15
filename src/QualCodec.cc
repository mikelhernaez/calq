/** @file QualCodec.cc
 *  @brief This file contains the implementations of the QualEncoder and
 *         QualDecoder classes.
 *  @author Jan Voges (voges)
 *  @bug No known bugs
 */

/*
 *  Changelog
 *  YYYY-MM-DD: What (who)
 */

#include "QualCodec.h"
#include "Common/constants.h"
#include "Common/Exceptions.h"
#include "Common/debug.h"
#include "Compressors/range/range.h"
#include "Compressors/rle/rle.h"
#include <limits>
#include <stdlib.h>

// Typical quality value ranges:
//   Sanger         Phred+33   [0,40]
//   Solexa         Solexa+64  [-5,40]
//   Illumina 1.3+  Phred+64   [0,40]
//   Illumina 1.5+  Phred+64   [0,40] with 0=unused, 1=unused, 2=Read Segment Quality Control Indicator ('B')
//   Illumina 1.8+  Phred+33   [0,41]

static const int QV_OFFSET_33 = 33;
static const int QV_OFFSET_64 = 64;
static const int QV_OFFSET = QV_OFFSET_33;
static const int QV_MIN = QV_OFFSET;
static const int QV_MAX = 104;

static const unsigned int QUANTIZER_STEP_MIN = 2;
static const unsigned int QUANTIZER_STEP_MAX = 8;
static const unsigned int QUANTIZER_NUM = QUANTIZER_STEP_MAX-QUANTIZER_STEP_MIN+1; // 2-8 steps
static const unsigned int QUANTIZER_IDX_MIN = 0;
static const unsigned int QUANTIZER_IDX_MAX = QUANTIZER_NUM-1;

QualEncoder::QualEncoder(File &cqFile,
                         const std::vector<FASTAReference> &fastaReferences,
                         const unsigned int &polyploidy)
    : fastaReferences(fastaReferences)
    , numBlocks(0)
    , numMappedRecords(0)
    , numUnmappedRecords(0)
    , cqFile(cqFile)
    , qi("")
    , qvi("")
    , uqv("")
    , reference("")
    , referencePosMin(std::numeric_limits<uint32_t>::max())
    , referencePosMax(std::numeric_limits<uint32_t>::min())
    , mappedRecordQueue()
    , observedNucleotides()
    , observedQualityValues()
    , observedPosMin(std::numeric_limits<uint32_t>::max())
    , observedPosMax(std::numeric_limits<uint32_t>::min())
    , genotyper(polyploidy, QUANTIZER_NUM, QUANTIZER_IDX_MIN, QUANTIZER_IDX_MAX, QV_MIN, QV_MAX)
    , uniformQuantizers()
    , quantizerIndices()
    , quantizerIndicesPosMin(std::numeric_limits<uint32_t>::max())
    , quantizerIndicesPosMax(std::numeric_limits<uint32_t>::min())
{
    // Init uniform quantizers
    std::cout << ME << "Initializing " << QUANTIZER_NUM << " uniform quantizers" << std::endl;
    for (unsigned int i = 0; i < QUANTIZER_NUM; i++) {
        unsigned int numberOfSteps = QUANTIZER_STEP_MIN;
        UniformQuantizer uniformQuantizer(QV_MIN, QV_MAX, numberOfSteps);
        //uniformQuantizer.print();
        uniformQuantizers.insert(std::pair<int,UniformQuantizer>(i, uniformQuantizer));
    }
}

QualEncoder::~QualEncoder(void)
{
    std::cout << ME << "Encoded " << numMappedRecords << " mapped record(s)"
              << " and " << numUnmappedRecords << " unmapped record(s)"
              << " in " << numBlocks << " block(s)" << std::endl;
}

void QualEncoder::startBlock(void)
{
    std::cout << ME << "Starting block " << numBlocks << std::endl;

    numMappedRecordsInBlock = 0;
    numUnmappedRecordsInBlock = 0;
    qi = "";
    qvi = "";
    uqv = "";
    reference = "";
    referencePosMin = std::numeric_limits<uint32_t>::max();
    referencePosMax = std::numeric_limits<uint32_t>::min();
    mappedRecordQueue = std::queue<MappedRecord>();
    observedNucleotides.clear();
    observedQualityValues.clear();
    observedPosMin = std::numeric_limits<uint32_t>::max();
    observedPosMax = std::numeric_limits<uint32_t>::min();
    quantizerIndices.clear();
    quantizerIndicesPosMin = std::numeric_limits<uint32_t>::max();
    quantizerIndicesPosMax = std::numeric_limits<uint32_t>::min(); 
}

void QualEncoder::addUnmappedRecordToBlock(const SAMRecord &samRecord)
{
    encodeUnmappedQualityValues(std::string(samRecord.qual));
    numUnmappedRecordsInBlock++;
    numUnmappedRecords++;
}

void QualEncoder::addMappedRecordToBlock(const SAMRecord &samRecord)
{
    // If the reference is empty, then this is the first mapped record being
    // added to the current block. If so, we load the corresponding reference
    // and initially set the min mapping positions for this block.
    if (reference.empty()) {
        loadFastaReference(std::string(samRecord.rname));
        observedPosMin = samRecord.pos - 1; // SAM format counts from 1
        quantizerIndicesPosMin = observedPosMin;
    }

    // Construct a mapped record from the given SAM record
    MappedRecord mappedRecord(samRecord);

    // Check if we need to update the max mapping position for records in this
    // block. If so, we also need to update the size of our observation vectors.
    if (mappedRecord.posMax > observedPosMax) {
        observedPosMax = mappedRecord.posMax;
        size_t observedSize = observedPosMax - observedPosMin + 1;
        observedNucleotides.resize(observedSize);
        observedQualityValues.resize(observedSize);
    }

    // Parse CIGAR string and assign nucleotides and QVs from the record 
    // (we name them 'observations') to their positions within the reference;
    // then push the current record to the queue.
    mappedRecord.extractObservations(observedPosMin, observedNucleotides, observedQualityValues);
    mappedRecordQueue.push(mappedRecord);

    // Check which genomic positions are ready for processing. Then compute
    // the quantizer indices for these positions and shrink the observation
    // vectors
    while (observedPosMin < mappedRecord.posMin) {
        //std::cerr << observedPosMin << ",";
        int k = genotyper.computeQuantizerIndex(observedNucleotides[0], observedQualityValues[0]);
        //std::cerr << k << std::endl;
        quantizerIndices.push_back(k);
        quantizerIndicesPosMax = observedPosMin;
        observedNucleotides.erase(observedNucleotides.begin());
        observedQualityValues.erase(observedQualityValues.begin());
        observedPosMin++;
    }

    // Check which records from the queue are ready for processing
    while (mappedRecordQueue.front().posMax < observedPosMin) {
        encodeMappedQualityValues(mappedRecordQueue.front());
        uint32_t currFirstPosition = mappedRecordQueue.front().posMin;
        mappedRecordQueue.pop();

        // Check if we can shrink the quantizer indices vector
        if (mappedRecordQueue.empty() == false) {
            uint32_t nextFirstPosition = mappedRecordQueue.front().posMin;
            if (nextFirstPosition > currFirstPosition) {
                quantizerIndices.erase(quantizerIndices.begin(), quantizerIndices.begin()+nextFirstPosition-currFirstPosition);
                quantizerIndicesPosMin += nextFirstPosition-currFirstPosition;
            }
        }
    }

    numMappedRecordsInBlock++;
    numMappedRecords++;
}

size_t QualEncoder::finishBlock(void)
{
    size_t ret = 0;

    std::cout << ME << "Finishing block " << numBlocks << std::endl;
    std::cout << ME << "  " << numMappedRecordsInBlock << " mapped record(s)" << std::endl;
    std::cout << ME << "  " << numUnmappedRecordsInBlock << " unmapped record(s)" << std::endl;

    // Compute all remaining quantizers
    while (observedPosMin <= observedPosMax) {
        //std::cerr << observedPosMin << ",";
        int k = genotyper.computeQuantizerIndex(observedNucleotides[0], observedQualityValues[0]);
        //std::cerr << k << std::endl;
        quantizerIndices.push_back(k);
        quantizerIndicesPosMax = observedPosMin;
        observedNucleotides.erase(observedNucleotides.begin());
        observedQualityValues.erase(observedQualityValues.begin());
        observedPosMin++;
    }

    // Process all remaining records from queue
    while (mappedRecordQueue.empty() == false) {
        encodeMappedQualityValues(mappedRecordQueue.front());
        uint32_t currFirstPosition = mappedRecordQueue.front().posMin;
        mappedRecordQueue.pop();

        // Check if we can shrink the quantizerIndices vector
        if (mappedRecordQueue.empty() == false) {
            uint32_t nextFirstPosition = mappedRecordQueue.front().posMin;
            if (nextFirstPosition > currFirstPosition) {
                quantizerIndices.erase(quantizerIndices.begin(), quantizerIndices.begin()+nextFirstPosition-currFirstPosition);
                quantizerIndicesPosMin += nextFirstPosition-currFirstPosition;
            }
        }
    }

    // RLE and range encoding for quantizer indices
    //std::cout << "Quantizer indices: " << qi << std::endl;
    unsigned char *qiBuffer = (unsigned char *)qi.c_str();
    size_t qiBufferSize = qi.length();
    if (qiBufferSize > 0) {
        size_t qiRLESize = 0;
        unsigned char *qiRLE = rle_encode(qiBuffer, qiBufferSize, &qiRLESize, QUANTIZER_NUM, (unsigned char)'0');

        size_t numQiBlocks = (size_t)ceil((double)qiRLESize / (double)MB);
        ret += cqFile.writeUint64(numQiBlocks);

        size_t encodedQiBytes = 0;
        while (encodedQiBytes < qiRLESize) {
            unsigned int bytesToEncode = 0;
            if ((qiRLESize - encodedQiBytes) > MB) {
                bytesToEncode = MB;
            } else {
                bytesToEncode = qiRLESize - encodedQiBytes;
            }

            unsigned int qiRangeSize = 0;
            unsigned char *qiRange = range_compress_o1(qiRLE+encodedQiBytes, (unsigned int)bytesToEncode, &qiRangeSize);

            if (qiRangeSize >= bytesToEncode) {
                ret += cqFile.writeUint8(0);
                ret += cqFile.writeUint32(bytesToEncode);
                ret += cqFile.write(qiRLE+encodedQiBytes, bytesToEncode);
            } else {
                ret += cqFile.writeUint8(1);
                ret += cqFile.writeUint32(qiRangeSize);
                ret += cqFile.write(qiRange, qiRangeSize);
            }

            encodedQiBytes += bytesToEncode;
            free(qiRange);
        }
        free(qiRLE);
    } else {
        ret += cqFile.writeUint64(0);
    }

    // RLE and range encoding for quality value indices
    //std::cout << "Quality value indices: " << qvi << std::endl;
    unsigned char *qviBuffer = (unsigned char *)qvi.c_str();
    size_t qviBufferSize = qvi.length();
    if (qviBufferSize > 0) {
        size_t qviRLESize = 0;
        unsigned char *qviRLE = rle_encode(qviBuffer, qviBufferSize, &qviRLESize, QUANTIZER_STEP_MAX, (unsigned char)'0');

        size_t numQviBlocks = (size_t)ceil((double)qviRLESize / (double)MB);
        ret += cqFile.writeUint64(numQviBlocks);

        size_t encodedQviBytes = 0;
        while (encodedQviBytes < qviRLESize) {
            unsigned int bytesToEncode = 0;
            if ((qviRLESize - encodedQviBytes) > MB) {
                bytesToEncode = MB;
            } else {
                bytesToEncode = qviRLESize - encodedQviBytes;
            }

            unsigned int qviRangeSize = 0;
            unsigned char *qviRange = range_compress_o1(qviRLE+encodedQviBytes, (unsigned int)bytesToEncode, &qviRangeSize);

            if (qviRangeSize >= bytesToEncode) {
                ret += cqFile.writeUint8(0);
                ret += cqFile.writeUint32(bytesToEncode);
                ret += cqFile.write(qviRLE+encodedQviBytes, bytesToEncode);
            } else {
                ret += cqFile.writeUint8(1);
                ret += cqFile.writeUint32(qviRangeSize);
                ret += cqFile.write(qviRange, qviRangeSize);
            }

            encodedQviBytes += bytesToEncode;
            free(qviRange);
        }
        free(qviRLE);
    } else {
        ret += cqFile.writeUint64(0);
    }

    // Range encoding for unmapped quality values
    //std::cout << "Unmapped quality values: " << uqv << std::endl;
    unsigned char *uqvBuffer = (unsigned char *)uqv.c_str();
    size_t uqvBufferSize = uqv.length();
    if (uqvBufferSize > 0) {
        size_t numUqvBlocks = (size_t)ceil((double)uqvBufferSize / (double)MB);
        ret += cqFile.writeUint64(numUqvBlocks);

        size_t encodedUqvBytes = 0;
        while (encodedUqvBytes < uqvBufferSize) {
            unsigned int bytesToEncode = 0;
            if ((uqvBufferSize - encodedUqvBytes) > MB) {
                bytesToEncode = MB;
            } else {
                bytesToEncode = uqvBufferSize - encodedUqvBytes;
            }

            unsigned int uqvRangeSize = 0;
            unsigned char *uqvRange = range_compress_o1(uqvBuffer+encodedUqvBytes, (unsigned int)bytesToEncode, &uqvRangeSize);

            if (uqvRangeSize >= bytesToEncode) {
                ret += cqFile.writeUint8(0);
                ret += cqFile.writeUint32(bytesToEncode);
                ret += cqFile.write(uqvBuffer+encodedUqvBytes, bytesToEncode);
            } else {
                ret += cqFile.writeUint8(1);
                ret += cqFile.writeUint32(uqvRangeSize);
                ret += cqFile.write(uqvRange, uqvRangeSize);
            }
 
            encodedUqvBytes += bytesToEncode;
            free(uqvRange);
        }
    } else {
        ret += cqFile.writeUint64(0);
    }

    //std::cout << ME << "Finished block " << numBlocks << std::endl;
    numBlocks++;

    return ret;
}

void QualEncoder::loadFastaReference(const std::string &rname)
{
    // Find FASTA reference for this RNAME
    std::cout << ME << "Searching FASTA reference for: " << rname << std::endl;
    bool foundFastaReference = false;

    for (auto const &fastaReference : fastaReferences) {
        if (fastaReference.header == rname) {
            if (foundFastaReference == true) {
                throwErrorException("Found multiple FASTA references");
            }
            foundFastaReference = true;
            reference = fastaReference.sequence;
            referencePosMin = 0;
            referencePosMax = (uint32_t)reference.size() - 1;
        }
    }

    if (foundFastaReference == true) {
        std::cout << ME << "Found FASTA reference" << std::endl;
    } else {
        throwErrorException("Could not find FASTA reference");
    }
}

void QualEncoder::encodeMappedQualityValues(const MappedRecord &mappedRecord)
{
    // Iterators
    uint32_t mrIdx = 0; // iterator for quality values in the mapped record
    uint32_t qiIdx = 0; // iterator for quantizer indices

    // Iterate through CIGAR string and code the quality values
    std::string cigar = mappedRecord.cigar;
    size_t cigarIdx = 0;
    size_t cigarLen = mappedRecord.cigar.length();
    size_t opLen = 0; // length of current CIGAR operation

    for (cigarIdx = 0; cigarIdx < cigarLen; cigarIdx++) {
        if (isdigit(cigar[cigarIdx])) {
            opLen = opLen*10 + (size_t)cigar[cigarIdx] - (size_t)'0';
            continue;
        }
        switch (cigar[cigarIdx]) {
        case 'M':
        case '=':
        case 'X':
            // Encode opLen quality values with computed quantizer indices
            for (size_t i = 0; i < opLen; i++) {
                int qualityValue = (int)mappedRecord.qualityValues[mrIdx++];
                int quantizerIndex = quantizerIndices[qiIdx++];
                int qualityValueIndex = uniformQuantizers.at(quantizerIndex).valueToIndex(qualityValue);
                qi += std::to_string(quantizerIndex);
                qvi += std::to_string(qualityValueIndex);
            }
            break;
        case 'I':
        case 'S':
            // Encode opLen quality values with max quantizer index
            for (size_t i = 0; i < opLen; i++) {
                int qualityValue = (int)mappedRecord.qualityValues[mrIdx++];
                int qualityValueIndex = uniformQuantizers.at(QUANTIZER_IDX_MAX).valueToIndex(qualityValue);
                qi += std::to_string(QUANTIZER_IDX_MAX);
                qvi += std::to_string(qualityValueIndex);
            }
            break;
        case 'D':
        case 'N':
            qiIdx += opLen;
            break; // do nothing as these bases are not present
        case 'H':
        case 'P':
            break; // these have been clipped
        default: 
            throwErrorException("Bad CIGAR string");
        }
        opLen = 0;
    }
}

void QualEncoder::encodeUnmappedQualityValues(const std::string &qualityValues)
{
    uqv += qualityValues;
}

QualDecoder::QualDecoder(File &cqFile,
                         File &qualFile,
                         const std::vector<FASTAReference> &fastaReferences)
    : cqFile(cqFile)
    , fastaReferences(fastaReferences)
    , qualFile(qualFile)
{

}

QualDecoder::~QualDecoder(void)
{
    // empty
}

size_t QualDecoder::decodeBlock(void)
{
    size_t ret = 0;

    std::string qiRLEString("");
    uint64_t numQiBlocks = 0;
    ret += cqFile.readUint64(&numQiBlocks);
    for (size_t i = 0; i < numQiBlocks; i++) {
        uint8_t qiRangeFlag = 0;
        ret += cqFile.readUint8(&qiRangeFlag);
        if (qiRangeFlag == 1) {
            uint32_t qiRangeSize = 0;
            ret += cqFile.readUint32(&qiRangeSize);
            unsigned char *qiRange = (unsigned char *)malloc(qiRangeSize);
            ret += cqFile.read(qiRange, qiRangeSize);
            unsigned int qiRLESize = 0;
            unsigned char *qiRLE= range_decompress_o1(qiRange, &qiRLESize);
            free(qiRange);
            std::string qiRLEStringTmp((char *)qiRLE, qiRLESize);
            free(qiRLE);
            qiRLEString += qiRLEStringTmp;
        } else { // qiRangeFlag == 0
            unsigned int qiRLESize = 0;
            ret += cqFile.readUint32(&qiRLESize);
            unsigned char *qiRLE = (unsigned char *)malloc(qiRLESize);
            ret += cqFile.read(qiRLE, qiRLESize);
            std::string qiRLEStringTmp((char *)qiRLE, qiRLESize);
            free(qiRLE);
            qiRLEString += qiRLEStringTmp;
        }

        size_t qiSize = 0;
        unsigned char *qi = rle_decode((unsigned char *)qiRLEString.c_str(), qiRLEString.length(), &qiSize, QUANTIZER_NUM, (unsigned int)'0');
        std::string qiString((char *)qi, qiSize);
        //std::cout << ME << "Decoded quantizer indices: " << qiString << std::endl;
        free(qi);
    }

    std::string qviRLEString("");
    uint64_t numQviBlocks = 0;
    ret += cqFile.readUint64(&numQviBlocks);
    for (size_t i = 0; i < numQviBlocks; i++) {
        uint8_t qviRangeFlag = 0;
        ret += cqFile.readUint8(&qviRangeFlag);
        if (qviRangeFlag == 1) {
            uint32_t qviRangeSize = 0;
            ret += cqFile.readUint32(&qviRangeSize);
            unsigned char *qviRange = (unsigned char *)malloc(qviRangeSize);
            ret += cqFile.read(qviRange, qviRangeSize);
            unsigned int qviRLESize = 0;
            unsigned char *qviRLE= range_decompress_o1(qviRange, &qviRLESize);
            free(qviRange);
            std::string qviRLEStringTmp((char *)qviRLE, qviRLESize);
            free(qviRLE);
            qviRLEString += qviRLEStringTmp;
        } else { // qviRangeFlag == 0
            unsigned int qviRLESize = 0;
            ret += cqFile.readUint32(&qviRLESize);
            unsigned char *qviRLE = (unsigned char *)malloc(qviRLESize);
            ret += cqFile.read(qviRLE, qviRLESize);
            std::string qviRLEStringTmp((char *)qviRLE, qviRLESize);
            free(qviRLE);
            qviRLEString += qviRLEStringTmp;
        }

        size_t qviSize = 0;
        unsigned char *qvi = rle_decode((unsigned char *)qviRLEString.c_str(), qviRLEString.length(), &qviSize, QUANTIZER_STEP_MAX, (unsigned int)'0');
        std::string qviString((char *)qvi, qviSize);
        //std::cout << ME << "Decoded quality value indices: " << qviString << std::endl;
        free(qvi);
    }

    std::string uqvString("");
    uint64_t numUqvBlocks = 0;
    ret += cqFile.readUint64(&numUqvBlocks);
    for (size_t i = 0; i < numUqvBlocks; i++) {
        uint8_t uqvRangeFlag = 0;
        ret += cqFile.readUint8(&uqvRangeFlag);
        if (uqvRangeFlag == 1) {
            uint32_t uqvRangeSize = 0;
            ret += cqFile.readUint32(&uqvRangeSize);
            unsigned char *uqvRange = (unsigned char *)malloc(uqvRangeSize);
            ret += cqFile.read(uqvRange, uqvRangeSize);
            unsigned int uqvSize = 0;
            unsigned char *uqv= range_decompress_o1(uqvRange, &uqvSize);
            free(uqvRange);
            std::string uqvStringTmp((char *)uqv, uqvSize);
            free(uqv);
            uqvString += uqvStringTmp;
        } else { // uqvRangeFlag == 0
            unsigned int uqvSize = 0;
            ret += cqFile.readUint32(&uqvSize);
            unsigned char *uqv = (unsigned char *)malloc(uqvSize);
            ret += cqFile.read(uqv, uqvSize);
            std::string uqvStringTmp((char *)uqv, uqvSize);
            free(uqv);
            uqvString += uqvStringTmp;
        }
        //std::cout << ME << "Decoded unmapped quality values: " << uqvString << std::endl;
    }

    return ret;
}

