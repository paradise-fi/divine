/*
 * state.h
 *
 *  Created on: 16.2.2010
 *      Author: Milan Krivanek
 */

#ifndef DIVINE_GENERATOR_COIN_STATE_H
#define DIVINE_GENERATOR_COIN_STATE_H

#include <cstring>
#include <vector>
#include <iostream>

#include <divine/toolkit/blob.h>

namespace divine {
namespace generator {

using std::size_t; //indexing
using std::vector;

typedef int value_t; //type of values
typedef unsigned char data_t; //type of raw data

/**
 * A singleton object containing the information about the
 * sizes of the individual elements in bytes.
 */
class StateMetrics {
public:

    /**
     * the number of elements
     */
    size_t dimension;

    /**
     * total size in bytes
     */
    size_t size;

    /**
     * offsets of the elements
     */
    vector<size_t> offsets;

    /**
     * sizes of the elements in bytes
     */
    vector<size_t> sizes;

    /**
     * Empty constructor
     */
    StateMetrics() :
        dimension(0), size(0) {
    }

    /**
     * Creates an object with constant byte size per element.
     *
     * \param dim               dimension of the state vector
     * \param bytes_per_value   size of one element in bytes
     */
    StateMetrics(size_t dim, size_t bytes_per_value = 1) :
        dimension(dim) {
        sizes.resize(dimension);
        offsets.resize(dimension);
        size = 0;
        for (size_t i = 0; i < dimension; i++) {
            sizes[i] = bytes_per_value;
            offsets[i] = size;
            size += sizes[i];
        }
    }

    /**
     * Creates a metrics object using a pre-initialized vector.
     *
     * \param dim   dimension of the state vector
     * \param v     vector containing sizes of the elements in bytes
     */
    StateMetrics(size_t dim, vector<size_t> & s) :
        dimension(dim) {
        sizes = s;
        offsets.resize(dimension);
        size = 0;
        for (size_t i = 0; i < dimension; i++) {
            offsets[i] = size;
            size += sizes[i];
        }
    }

};

class State {

private:
    /**
     * Vector of states of the simple automata
     */
    vector<value_t> product_state;

public:

    static StateMetrics metrics;

    /**
     * Empty constructor
     */
    State() {
    }

    /**
     * Constructs a state from the data in a Blob
     *
     * \param b the Blob
     * \param slack the slack space
     */
    State(Blob &b, int slack) {
        product_state.clear();
        product_state.reserve(metrics.dimension);
        data_t * data = &b.get<data_t> (slack);
        for (size_t i = 0; i < metrics.dimension; i++) {
            product_state.push_back(getPrimitiveState(data, i));
        }
    }

    /**
     * Constructs a state using the provided vector of values;
     * the vector must match State::metrics
     */
    State(vector<value_t> &v) { //value or reference?
        product_state = v;
    }

    /**
     * Get the value of the n-th component of the state packed in the Blob
     * using the provided slack space.
     */
    static value_t getPrimitiveState(Blob &b, int slack, size_t n) {
        data_t * data = &b.get<data_t> (slack);
        return getPrimitiveState(data, n);
    }

    /**
     * Pack the current state into a provided Blob with the given slack space.
     */
    void pack(Blob &b, int slack) {
        data_t * data = &b.get<data_t> (slack);
        for (size_t i = 0; i < metrics.dimension; i++) {
            setPrimitiveState(data, i, product_state[i]);
        }
    }

    /**
     * Returns the vector of values representing the current state.
     *
     * TODO:
     * Return a copy of the vector for safety,
     * or a reference for performance? Is a reference really faster?
     */
    const vector<value_t> & getValues() {
        return product_state;
    }

    /**
     * Returns the offset in the array of raw bytes containing n-th element
     *
     * \param n     index
     * \return      offset of n-th element
     */
    static size_t getOffset(size_t n) {
        return metrics.offsets[n];
    }

    /**
     * Returns the size of the n-th element in bytes.
     *
     * \param n     index
     * \return      size of n-th element
     */
    static size_t getSize(size_t n) {
        return metrics.sizes[n];
    }

    /**
     * Returns the total size of raw data representing a state in bytes
     *
     * \return size of a state in bytes
     */
    static size_t totalSize() {
        return metrics.size;
    }

    /**
     * Returns the dimension of the state vector
     *
     * \return  dimension of the state vector
     */
    static size_t length() {
        return metrics.dimension;
    }

    /**
     * Initialize states using the provided StateMetrics.
     *
     * \param m     metrics
     */
    static void init(StateMetrics &m) {
        State::metrics = m;
    }

    /**
     * Prints a state.
     */
    friend std::ostream& operator<<(std::ostream &out, State &s) {
        vector<value_t> values = s.product_state;
        out << '[';
        for (size_t i = 0; i < values.size(); i++) {
            out << values[i];
            if (i + 1 < values.size()) {
                out << ", ";
            }
        }
        out << ']';
        return out;
    }

private:

    /**
     * Converts first <code>count</code> bytes from the provided data into an integer.
     *
     * \param bytes     pointer to the data
     * \param count     the number of bytes to be converted
     */
    static value_t getInt(data_t *bytes, int count) {
        value_t ret = 0;
        for (int i = 0; i < count; ++i) {
            ret |= (bytes[i] << (8 * i));
        }
        return ret;
    }

    /**
     * Sets the first bytes of the data to contain the value <code>val</code>.
     * The data must fit into <code>count</code> bytes.
     *
     * \param value   the value to be stored
     * \param bytes pointer to the raw data to be overwritten
     * \param count maximum number of bytes to be overwritten
     */
    static void setBytes(value_t value, data_t *bytes, int count) {
        for (int i = 0; i < count; ++i) {
            bytes[i] = ((value >> (8 * i)) & 0xFF);
        }
    }

    /**
     * Modifies the n-th element in the raw data to contain the specified value.
     *
     * \param buff      raw data
     * \param n         index
     * \param value     new value
     */
    static void setPrimitiveState(data_t * buff, value_t n, value_t value) {
        setBytes(value, buff + getOffset(n), getSize(n));
    }

    /**
     * Extracts the value of the n-th element from the raw data.
     *
     * \param buff      raw data
     * \param n         index
     */
    static value_t getPrimitiveState(data_t * buff, size_t n) {
        return getInt(buff + getOffset(n), getSize(n));
    }

};

} //namespace generator
} //namespace divine

#endif /* DIVINE_GENERATOR_COIN_NODE_H */
