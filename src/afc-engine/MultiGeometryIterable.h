// Copyright (C) 2017 RKF Engineering Solutions, LLC
#ifndef MULTI_GEOMETRY_ITERABLE_H
#define MULTI_GEOMETRY_ITERABLE_H

#include <ogr_geometry.h>
#include <stdexcept>
#include <type_traits>

/** Wrap an OGRMultiPoint object and allow STL-style iteration.
 */
template<class MemberGeom, class Container=OGRGeometryCollection>
class MultiGeometryIterable{
public:
    /** Create a new iterable wrapper.
     *
     * @param geom The multi-geometry to iterate over.
     */
    MultiGeometryIterable(Container &geom)
     : _geom(&geom){}

    /// Shared iterator implementation
    template<class Contained>
    class base_iterator{
    public:
        /** Define a new iterator.
         *
         * @param geom The geometry to access.
         * @param index The point index to access.
         */
        base_iterator(Container &geom, int index)
         : _geom(&geom), _ix(index){}


        /** Dereference to a single item pointer.
         *
         * @return The item at this iterator.
         */
        Contained * operator * () const{
            auto *sub = _geom->getGeometryRef(_ix);
            if(!sub){
                throw std::logic_error("MultiGeometryIterable null dereference");
            }
            return static_cast<Contained *>(sub);
        }

        /** Determine if two iterators are identical.
         *
         * @param other The iterator to compare against.
         * @return True if the other iterator is the same.
         */
        bool operator == (const base_iterator &other) const{
            return ((_geom == other._geom) && (_ix == other._ix));
        }

        /** Determine if two iterators are identical.
         *
         * @param other The iterator to compare against.
         * @return True if the other iterator is different.
         */
        bool operator != (const base_iterator &other) const{
            return !operator==(other);
        }

        /** Pre-increment operator.
         *
         * @return A reference to this incremented object.
         */
        base_iterator & operator ++ (){
            ++_ix;
            return *this;
        }

    private:
        /// External geometry being accessed
        Container *_geom;
        /// Geometry index
        int _ix;
    };

    /// Iterator for mutable containers
    typedef base_iterator<MemberGeom> iterator;
    /// Iterator for immutable containers
    typedef base_iterator<const MemberGeom> const_iterator;

    iterator begin(){
        return iterator(*_geom, 0);
    }
    iterator end(){
        return iterator(*_geom, _geom->getNumGeometries());
    }

private:
    /// Reference to the externally-owned object
    Container *_geom;
};

template<class MemberGeom>
class MultiGeometryIterableConst
    : public MultiGeometryIterable<const MemberGeom, const OGRGeometryCollection>{
public:
    MultiGeometryIterableConst(const OGRGeometryCollection &geom)
     : MultiGeometryIterable<const MemberGeom, const OGRGeometryCollection>(geom){}
};

template<class MemberGeom>
class MultiGeometryIterableMutable
    : public MultiGeometryIterable<MemberGeom, OGRGeometryCollection>{
public:
    MultiGeometryIterableMutable(OGRGeometryCollection &geom)
     : MultiGeometryIterable<MemberGeom, OGRGeometryCollection>(geom){}
};

#endif /* MULTI_GEOMETRY_ITERABLE_H */
