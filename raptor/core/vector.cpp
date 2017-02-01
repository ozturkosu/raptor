// Copyright (c) 2015, Raptor Developer Team, University of Illinois at Urbana-Champaign
// License: Simplified BSD, http://opensource.org/licenses/BSD-2-Clause
#include "vector.hpp"

using namespace raptor;

/**************************************************************
*****   Vector Set Constant Value
**************************************************************
***** Initializes the vector to a constant value
*****
***** Parameters
***** -------------
***** alpha : data_t
*****    Constant value to set each element of vector to
**************************************************************/
void Vector::set_const_value(data_t alpha)
{
    for (index_t i = 0; i < size; i++)
    {
        values[i] = alpha;
    }
}

/**************************************************************
*****   Vector Set Random Values
**************************************************************
***** Initializes each element of the vector to a random
***** value
**************************************************************/
void Vector::set_rand_values()
{
    srand(time(NULL));
    for (index_t i = 0; i < size; i++)
    {
        values[i] = rand() / RAND_MAX;
    }
}

/**************************************************************
*****   Vector AXPY
**************************************************************
***** Multiplies the vector by a constant, alpha, and then
***** sums each element with corresponding entry of Y
*****
***** Parameters
***** -------------
***** y : Vector&
*****    Vector to be summed with
***** alpha : data_t
*****    Constant value to multiply each element of vector by
**************************************************************/
void Vector::axpy(Vector& x, data_t alpha)
{
    for (index_t i = 0; i < size; i++)
    {
        values[i] += x.values[i]*alpha;
    }
}

/**************************************************************
*****   Vector Scale
**************************************************************
***** Multiplies each element of the vector by a constant value
*****
***** Parameters
***** -------------
***** alpha : data_t
*****    Constant value to set multiply element of vector by
**************************************************************/
void Vector::scale(data_t alpha)
{
    for (index_t i = 0; i < size; i++)
    {
        values[i] *= alpha;
    }
}

/**************************************************************
*****   Vector Norm
**************************************************************
***** Calculates the P norm of the vector (for a given P)
*****
***** Parameters
***** -------------
***** p : index_t
*****    Determines which p-norm to calculate
**************************************************************/
data_t Vector::norm(index_t p)
{
    data_t result = 0.0;
    for (index_t i = 0; i < size; i++)
    {
        result += pow(values[i], p);
    }
    return pow(result, 1.0/p);
}

/**************************************************************
*****   Print Vector
**************************************************************
***** Prints all nonzero elements in vector
*****
***** Parameters
***** -------------
***** vec_name : const char* (optional)
*****    Name to be printed.  Default prints Vec[%d] = %e.
**************************************************************/
void Vector::print(const char* vec_name)
{
    for (int i = 0; i < size; i++)
    {
        if (fabs(values[i]) > zero_tol)
            printf("%s[%d] = %e\n", vec_name, i, values[i]);
    }
}

/**************************************************************
*****   Vector Data
**************************************************************
***** Returns pointer to vector entries
*****
***** Returns
***** -------------
***** data_t*
*****    Pointer to values of vector
**************************************************************/
data_t* Vector::data()
{
    return values.data();
}


