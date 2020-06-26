#ifndef SOLVER_H
#define SOLVER_H

#include <vector>
#include <bitset>
#include <iostream>
#include <math.h>
#include <unordered_set>
#include <set>
#include <map>
#include <limits>
#include <assert.h>
#include <algorithm>

class Solver
{
public:
    bool debug = false;
    typedef std::vector<uint64_t> Matrix;
    struct Solution
    {
        bool exists = false;
        std::set<size_t> involvedBits;
        std::set<size_t> uninvolvedBits;
        std::set<size_t> unknownBits;
    };

    void solve(Matrix &matrix, size_t colMax);
    Solution getSolution(const std::vector<uint64_t> &matrix);
    void printMatrix(const Matrix &matrix);

private:
    static constexpr size_t bitSize = sizeof (size_t)*8;
    std::set<size_t> getSetBits(std::bitset<bitSize> bits);
    size_t getSetBit(std::bitset<bitSize> bits);
    size_t findRowToSwap(const Matrix &matrix, size_t row, size_t col);
    void xorRows(Matrix &matrix, size_t row, size_t col);
    void swapRows(Matrix &matrix, size_t r1, size_t r2);
};

#endif // SOLVER_H
