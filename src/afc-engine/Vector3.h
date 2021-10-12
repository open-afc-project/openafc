#ifndef VECTOR_H
#define VECTOR_H

#include <armadillo>
#include <math.h>
#include <iostream>
#include <QDebug>

#include "MathHelpers.h"

using namespace arma;
using namespace MathHelpers;

/**
 * A class representing a 3-Dimensional Vector in floating point precision
 * 
 */
class Vector3 {
public:
    /** 
     * Construct a vector from its 3 coordinates
     * 
     * @param x X coordinate to set
     * @param y Y coordinate to set
     * @param z Z coordinate to set
     */
	Vector3(double x = 0.0, double y = 0.0, double z = 0.0) {
        data[0] = x;
		data[1] = y;
		data[2] = z;
	}

    /** 
     * Copy constructor
     * 
     * @param other Vector to copy
     */
	Vector3(const Vector3 &other) : data(other.data) {}

    /** 
     * Construct Vector from underlying armadillo vector
     * 
     * @param vec Armadillo 3 point vector
     */
	Vector3(const arma::vec3 &vec) : data(vec) {}

    /** 
     * Get the x coordinate
     * 
     * @return x coordinate
     */
	inline double x() const {
		return data[0];
	}

    /** 
     * Get the y coordinate
     * 
     * @return y coordinate
     */
	inline double y() const {
		return data[1];
	}

    /** 
     * Get the z coordinate
     * 
     * @return z coordinate
     */
	inline double z() const {
		return data[2];
	}

    inline void normalize() {
        double l = len();
        data /= l;
    }
    
    
    /** 
     * Perform a cross-product of vector with other vector and return resulting vector
     * 
     * @param other The other vector to perform cross product with
     * 
     * @return The resulting vector
     */
	inline Vector3 cross(const Vector3 &other) const {
		return Vector3(data[1] * other.data[2] - data[2] * other.data[1], data[2] * other.data[0] - data[0] * other.data[2], data[0] * other.data[1] - data[1] * other.data[0]);
	}

    /** 
     * Perform a dot-product of vector with other vector and return result
     * 
     * @param other The other vector to perform dot product with
     * 
     * @return The result of dot product
     */
	inline double dot(const Vector3 &other) const {
		return arma::dot(this->data, other.data);
	}

    /** 
     * Perform a dot-product of vector with other vector after normalizing each
     * to have length one
     * 
     * @param other The other vector to perform dot product with
     * 
     * @return The result of the dot product
     */
	inline double normDot(const Vector3 &other) const {
		return arma::norm_dot(this->data, other.data);
	}

    /** 
     * The total length of this vector as a 3-D cartesian vector
     * 
     * @return The length
     */
	inline double len() const {
		return sqrt(sqr(data[0]) + sqr(data[1]) + sqr(data[2]));
	}

    /** 
     * Get a copy of this vector but normalized to have length of one
     * 
     * @return normalize vector
     */
	inline Vector3 normalized() const {
		return Vector3(data / len());
	}

    /** 
     * The angle between this vector and other vector
     * 
     * @param other The other vector
     * 
     * @return angle in radians
     */
	inline double angleBetween(const Vector3 &other) const {
		return acos(dot(other) / (len() * other.len()));
	}

	inline Vector3 operator + (const Vector3 &other) const {
		return Vector3(data + other.data);
	}

	inline Vector3 operator - (const Vector3 &other) const {
		return Vector3(data - other.data);
	}

	inline Vector3 operator - () const {
		return Vector3(-data);
	}

	inline Vector3 operator * (const Vector3 &other) const {
		return Vector3(data % other.data);
	}

	inline Vector3 operator / (const Vector3 &other) const {
		return Vector3(data / other.data);
	}

	inline Vector3 operator * (const double scalar) const {
		return Vector3(data * scalar);		
	}

	inline friend Vector3 operator * (const double scalar, const Vector3 &vector) {
		return vector * scalar;	
	}

	inline Vector3 operator = (const Vector3 &other) {
		data = other.data;
		return *this;
	}

	inline bool operator == (const Vector3 &other) {
                bool retval = (data[0] == other.data[0]) && (data[1] == other.data[1]) && (data[2] == other.data[2]);

		return retval;
	}
	friend std::ostream& operator<< (std::ostream &os, const Vector3 &vector) {
		return os << "(" << vector.x() << ", " << vector.y() << ", " << vector.z() << ")";
	}

    friend QDebug operator<<(QDebug stream, const Vector3 &vector){
        stream.nospace() << "(" << vector.x() << ", " << vector.y() << ", " << vector.z() << ")";
        return stream.space();
    }

protected:
	arma::vec3 data;
};

#endif
