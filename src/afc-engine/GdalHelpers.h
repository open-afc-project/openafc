#ifndef SRC_KSCEGEOMETRY_GDALHELPERS_H_
#define SRC_KSCEGEOMETRY_GDALHELPERS_H_

#include <string>
#include <memory>

class OGRGeometry;
class OGRPoint;
class OGRMultiPoint;
class OGRLineString;
class OGRMultiLineString;
class OGRLinearRing;
class OGRPolygon;
class OGRMultiPolygon;
class OGRGeometryCollection;
class OGRFeature;
class OGRSpatialReference;

namespace GdalHelpers{

	/** A helper function to construct new OGRGeometry-derived objects in the
	 * proper dynamic library memory space (specifically for windows DLLs).
	 *
	 * @tparam Typ A class derived from OGRGeometry.
	 * @return A new default instance of the desired class.
	 */
	template<class Typ>
		Typ * createGeometry() = delete;

	template<>
		OGRPoint * createGeometry();

	template<>
		OGRMultiPoint * createGeometry();

	template<>
		OGRLineString * createGeometry();

	template<>
		OGRMultiLineString * createGeometry();

	template<>
		OGRLinearRing * createGeometry();

	template<>
		OGRPolygon * createGeometry();

	template<>
		OGRMultiPolygon * createGeometry();

	template<>
		OGRGeometryCollection * createGeometry();

	/** Delete OGR data with OGRFree() in a DLL-safe way.
	*/
	class OgrFreer{
		public:
			/// Interface for std::unique_ptr deleter
			void operator()(void *ptr) const;
	};

	/** Delete OGRGeometry objects in a DLL-safe way.
	*/
	class GeometryDeleter{
		public:
			/// Interface for std::unique_ptr deleter
			void operator()(OGRGeometry *ptr) const;
	};

	/// Convenience name for unique-pointer class
	template<class T=OGRGeometry>
		using GeomUniquePtr = std::unique_ptr<T, GeometryDeleter>;

	/** Delete OGRFeature objects in a DLL-safe way.
	*/
	class FeatureDeleter{
		public:
			/// Interface for std::unique_ptr deleter
			void operator()(OGRFeature *obj);
	};

	/** Delete OGRSpatialReference objects in a DLL-safe way.
	*/
	class SrsDeleter{
		public:
			/// Interface for std::unique_ptr deleter
			void operator()(OGRSpatialReference *obj);
	};

	/** Move contained geometries from one collection to another.
	 * @param target The target into which all geometries are moved.
	 * @param source The source from which geometries are taken.
	 * @post The @c source is empty.
	 */
	void coalesce(OGRGeometryCollection *target, OGRGeometryCollection *source);

	/** Export geometry to a Well Known Binary representation.
	 *
	 * @param geom The geometry object to export.
	 * @return The WKB byte string for the geometry.
	 * The data is in network byte order (big-endian).
	 */
	std::string exportWkb(const OGRGeometry &geom);

	/** Export geometry to Well Known Text representation.
	 * A null geometry has an empty array.
	 *  @param geom The object to export, which may be null.
	 *  Ownership is kept by the caller.
	 *  @return The WKT string for the geometry.
	 */
	std::string exportWkt(const OGRGeometry *geom);

	/** Import geometry from Well Known Text representation.
	 * An empty array will result in a null geometry.
	 * @param data The WKT string for the geometry.
	 * @return The new created geometry.
	 * Ownership is taken by the caller.
	 */
	OGRGeometry * importWkt(const std::string &data);

	/** Export SRS to Well Known Text representation.
	 *  @param srs Pointer to the object to export, which may be null.
	 *  @return The WKT string for the SRS.
	 */
	std::string exportWkt(const OGRSpatialReference *srs);

	/** Export SRS to Proj.4 representation.
	 *  @param srs Pointer to the object to export, which may be null.
	 *  @return The proj.4 string for the SRS.
	 */
	std::string exportProj4(const OGRSpatialReference *srs);

	OGRSpatialReference * importWellKnownGcs(const std::string &name);

}

#endif /* SRC_KSCEGEOMETRY_GDALHELPERS_H_ */
