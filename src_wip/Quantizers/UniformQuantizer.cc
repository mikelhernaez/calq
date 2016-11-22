/** @file UniformQuantizer.cc
 *  @brief This file contains the implementation of the UniformQuantizer class.
 *  @author Jan Voges (voges)
 *  @bug No known bugs
 */

/*
 *  Changelog
 *  YYYY-MM-DD: What (who)
 */

#include "UniformQuantizer.h"
#include "Common/Exceptions.h"
#include "Common/helpers.h"
#include <cmath>
#include <queue>
#include <vector>

UniformQuantizer::UniformQuantizer(const int &minimumValue,
                                   const int &maximumValue,
                                   const unsigned int &numberOfSteps)
    : lut()
    , inverseLut()
{
    // Sanity checks
    if ((minimumValue >= maximumValue) || (numberOfSteps <= 1)) {
        throwErrorException("Error in quantizer initialization");
    }

    // Compute the step size
    double stepSize = (maximumValue - minimumValue) / numberOfSteps;

    // Compute the borders and the representative values
    std::queue<double> borders;
    std::queue<int> reconstructionValues;
    double newBorder = minimumValue;

    borders.push(minimumValue);
    reconstructionValues.push(minimumValue + round(stepSize/2));
    for (unsigned i = 0; i < numberOfSteps-1; i++) {
        newBorder += stepSize;
        borders.push(newBorder);
        reconstructionValues.push(newBorder + round(stepSize/2));
    }
    borders.push(maximumValue);

    // Fill the quantization table
    borders.pop();
    int currentIndex = 0;
    int currentReconstructionValue = reconstructionValues.front();
    double currentBorder = borders.front();
    for (int value = minimumValue; value <= maximumValue; value++) {
        if (value > currentBorder) {
            currentIndex++;
            reconstructionValues.pop();
            borders.pop();
            currentReconstructionValue = reconstructionValues.front();
            currentBorder = borders.front();
        }
        std::pair<int,int> curr(currentIndex, currentReconstructionValue);
        lut.insert(std::pair<int,std::pair<int,int>>(value, curr));
        inverseLut.insert(curr);
    }
}

UniformQuantizer::~UniformQuantizer(void)
{
    // empty
}

int UniformQuantizer::valueToIndex(const int &value)
{
    if (lut.find(value) == lut.end()) {
        throwErrorException("Value out of range for quantizer");
    }

    return lut[value].first;
}

int UniformQuantizer::indexToReconstructionValue(const int &index)
{
    if (inverseLut.find(index) == inverseLut.end()) {
        throwErrorException("Quantization index not found");
    }

    return inverseLut[index];
}

int UniformQuantizer::valueToReconstructionValue(const int &value)
{
    if (lut.find(value) == lut.end()) {
        throwErrorException("Value out of range for quantizer");
    }

    return lut[value].second;
}

void UniformQuantizer::print(void) const
{
    std::cout << "LUT:" << std::endl;
    for (auto const &lutEntry : lut) {
        std::cout << "  " << lutEntry.first << ": ";
        std::cout << lutEntry.second.first << ",";
        std::cout << lutEntry.second.second << std::endl;
    }

    std::cout << "Inverse LUT:" << std::endl;
    for (auto const &inverseLutEntry : inverseLut) {
        std::cout << "  " << inverseLutEntry.first << ": ";
        std::cout << inverseLutEntry.second << std::endl;
    }
}
