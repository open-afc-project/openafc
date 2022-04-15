#include <QString>
#include <QRectF>
#include <vector>
#include <utility>
#include "gdal/gdal_priv.h"

struct RasterModel
{
	QRectF bounds;  // in lon/lat, with starting point in top left
	double xres;    // degrees longitude per pixel
	double yres;    // degrees latitude per pixel
	double nodata;  // special no data value
	GDALDataset* model; // file resource
};

class BuildingRasterModel
{
	public:

		enum HeightResult
		{
			OUTSIDE_REGION,
			NO_BUILDING,
			BUILDING
		};

		// Constructor for building raster file. Takes the directory where GeoTiff files are stored. Bounding box parameters are optional.
		BuildingRasterModel(const QString& modelDir, double minlat=-90, double minlon=-180, double maxlat=90, double maxlon=180);
		~BuildingRasterModel();

		// Returns building height at a givin (lat/lon) point. If there are no buildings present at the given position then a quiet NaN value is returned
		std::pair<BuildingRasterModel::HeightResult, double> getHeight(const double& latDeg, const double& lonDeg) const;

		// returns the boundaries of the raster tiles as QRectFs
		std::vector<QRectF> getBounds() const;

		// getHeight() return value to use if data is inside boundary but there is no building data.
		// Doesn't matter what the value is, just needs to be outside the range of valid building heights an not a NaN val.
		static const double nodata;  

	private:

		QRectF bounds;
		std::vector<RasterModel> models;
};

