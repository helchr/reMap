#include "solver.h"

void Solver::swapRows(Matrix& matrix, size_t r1, size_t r2)
{
    std::swap(matrix[r1],matrix[r2]);
}

void Solver::xorRows(Matrix& matrix, size_t row, size_t col)
{
    auto tmp = matrix[row];
    uint64_t mask = 1ULL << col;
    for(size_t r = 0; r < matrix.size(); r++)
    {
        if(r != row)
        {
            if((matrix[r] & mask) == mask)
            {
                matrix[r] ^= tmp;
            }
        }
    }
}

size_t Solver::findRowToSwap(const Matrix& matrix, size_t row, size_t col)
{
    uint64_t mask = 1ULL << col;
    for(size_t r = row+1; r < matrix.size(); r++)
    {
        if((matrix[r] & mask) == mask)
        {
            return r;
        }
    }
    return row;
}


void Solver::printMatrix(const Matrix& matrix)
{
    for(auto row : matrix)
    {
        std::cout << std::bitset<bitSize>(row) << std::endl;
    }
    std::cout << std::endl;
}

size_t Solver::getSetBit(std::bitset<bitSize> bits)
{
    for(size_t i = 0; i < bitSize; i++ )
    {
        if(bits[i] == 1)
        {
            return i;
        }
    }
    return std::numeric_limits<size_t>::max();
}

std::set<size_t> Solver::getSetBits(std::bitset<bitSize> bits)
{
    std::set<size_t> out;
    assert(bits.count() > 1);
    for(size_t i = 0; i < bitSize; i++ )
    {
        if(bits[i] == 1)
        {
            out.insert(i);
        }
    }
    return out;
}



void Solver::solve(Matrix& matrix, size_t colMax)
{
    for(size_t c = colMax, r = 0; c-- > 1 && r < matrix.size();)
    {
        uint64_t mask = 1ULL << c;
        if((matrix[r] & mask) == 1)
        {
            xorRows(matrix,r,c);
            r++;
        }
        else
        {
            auto rowToSwap = findRowToSwap(matrix,r,c);
            if(rowToSwap == r)
            {
                //move to next column, stay in same row
                continue;
            }
            else
            {
                swapRows(matrix,r,rowToSwap);
                xorRows(matrix,r,c);
                r++;
            }
        }
        if(debug)
        {
            printMatrix(matrix);
        }
    }
}

Solver::Solution Solver::getSolution(const std::vector<uint64_t> &matrix)
{
    Solution s;
    s.exists = true;
    for(auto row : matrix)
    {
        std::bitset<bitSize> bits(row>>1);
        std::bitset<1> rhs(row&1);
        if(bits.count() == 0 && rhs == 1)
        {
            // all zeros but a one in the last column
            std::cout << "No solution exists" << std::endl;
            s.exists=false;
            break;
        }
        else if(bits.count() == 1 && rhs == 0)
        {
           s.uninvolvedBits.insert(getSetBit(bits));
        }
        else if(bits.count() == 1 && rhs == 1)
        {
           s.involvedBits.insert(getSetBit(bits));
        }
        else if(bits.count() >= 2)
        {
            auto setBits = getSetBits(bits);
            s.unknownBits.insert(setBits.begin(),setBits.end());
        }
    }
    if(s.exists)
    {
        std::set<uint64_t> common;
        std::set_intersection(s.involvedBits.begin(),s.involvedBits.end(),
                              s.uninvolvedBits.begin(),s.uninvolvedBits.end(),
                              std::inserter(common,common.begin()));
        for(auto c : common)
        {
            auto it = s.involvedBits.find(c);
            s.involvedBits.erase(it);
            it = s.uninvolvedBits.find(c);
            s.uninvolvedBits.erase(it);
        }
    }
    return s;
}
