#include "GdalImageMap.h"
#include "GdalImageFile.h"

#include <QPointF>
#include <QRectF>

GdalImageMap::GdalImageMap(const QList<QString> &filenames, const GeodeticCoord &topRight, const GeodeticCoord &bottomLeft){
	QRectF coverRect(QPointF(bottomLeft.longitudeDeg, bottomLeft.latitudeDeg),
			QPointF(topRight.longitudeDeg, topRight.latitudeDeg));
	foreach(const QString &fn, filenames){
		GdalImageFile *testFile = new GdalImageFile(fn);
		GeodeticCoord tr, bl;
		tr = testFile->topRight();
		bl = testFile->bottomLeft();
		QRectF rf(QPointF(bl.longitudeDeg, bl.latitudeDeg),
				QPointF(tr.longitudeDeg, tr.latitudeDeg));

		if(coverRect.intersects(rf)){
			testFile->loadData();
			_files << testFile;
			_topRights << tr;
			_bottomLefts << bl;
		}
		else delete testFile;
	}
}

double GdalImageMap::getValue(const GeodeticCoord &gc) const {
	for(int i = 0; i < _files.count(); ++i){
		const GeodeticCoord &bl = _bottomLefts.at(i);
		const GeodeticCoord &tr = _topRights.at(i);

		if(bl.latitudeDeg < gc.latitudeDeg && gc.latitudeDeg < tr.latitudeDeg &&
				bl.longitudeDeg < gc.longitudeDeg && gc.longitudeDeg < tr.longitudeDeg){
			return _files.at(i)->getValue(gc);
		}
	}

	return GdalImageFile::NO_DATA;
}

void GdalImageMap::printBB() const {
	for(int i = 0; i < _files.count(); ++i){
		const GeodeticCoord &bl = _bottomLefts.at(i);
		const GeodeticCoord &tr = _topRights.at(i);

		// std::cout << "REGION: " << i << std::endl;
		// std::cout << "    LON: " << bl.longitudeDeg << " " << tr.longitudeDeg << std::endl;
		// std::cout << "    LAT: " << bl.latitudeDeg << " " << tr.latitudeDeg << std::endl;

		std::cout << bl.longitudeDeg << " " << bl.latitudeDeg << std::endl;
		std::cout << tr.longitudeDeg << " " << bl.latitudeDeg << std::endl;
		std::cout << tr.longitudeDeg << " " << tr.latitudeDeg << std::endl;
		std::cout << bl.longitudeDeg << " " << tr.latitudeDeg << std::endl;
		std::cout << bl.longitudeDeg << " " << bl.latitudeDeg << std::endl;
		std::cout << std::endl;
	}

	return;
}
