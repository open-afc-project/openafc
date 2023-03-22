/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */

#include <assert.h>
#include "GzipCsv.h"
#include "FileHelpers.h"
#include <boost/lexical_cast.hpp>
#include <rkflogging/Logging.h>
#include <stdexcept>

namespace {
	// Logger for all instances of class
	LOGGER_DEFINE_GLOBAL(logger, "GzipCsv")

} // end namespace

GzipCsv::GzipCsv(const std::string &filename)
{
	if (filename.empty()) {
		return;
	}
	LOGGER_INFO(logger) << "Opening '" << QString::fromStdString(filename) << "'";
	QString qfilename = QString::fromStdString(filename);
	_fileWriter = FileHelpers::open(qfilename, QIODevice::WriteOnly);
	_gzipWriter.reset(new GzipStream(_fileWriter.get()));
	if (!_gzipWriter->open(QIODevice::WriteOnly))
	{
		throw std::runtime_error(
			QString("Gzip \"%1\" failed to open").arg(qfilename).toStdString());
	}
	_csvWriter.reset(new CsvWriter(*_gzipWriter));
}

GzipCsv::operator bool() const
{
	return (bool)_fileWriter;
}

void GzipCsv::clearRow()
{
	for (auto &colDef : _columns) {
		colDef->resetValue();
	}
}

void GzipCsv::completeRow()
{
	if (!*this) {
		return;
	}
	if (!_headingWritten) {
		_headingWritten = true;
		for (auto &colDef : _columns) {
			_csvWriter->writeRecord(colDef->name());
		}
		_csvWriter->writeEndRow();
	}
	for (auto &colDef : _columns) {
		_csvWriter->writeRecord(colDef->formatValue());
	}
	clearRow();
	_csvWriter->writeEndRow();
}

void GzipCsv::writeRow(const std::vector<std::string> &columns)
{
	for (const auto &col: columns) {
		_csvWriter->writeRecord(QString::fromStdString(col));
	}
	_csvWriter->writeEndRow();
}


void GzipCsv::addColumn(ColBase *column)
{
	_columns.push_back(column);
}

///////////////////////////////////////////////////////////////////////////////

GzipCsv::ColBase::ColBase(GzipCsv *container, const std::string &name)
	: _name(QString::fromStdString(name))
{
	container->addColumn(this);
}

void GzipCsv::ColBase::checkSet() const
{
	if (_valueSet) {
		return;
	}
	throw std::runtime_error(
		QString("Attempt to read value from column \"%1\" that was not set yet").
		arg(_name).toStdString());
}

///////////////////////////////////////////////////////////////////////////////

GzipCsv::ColInt::ColInt(GzipCsv *container, const std::string &name)
	: ColBase(container, name)
{}

QString GzipCsv::ColInt::formatValue() const
{
	return isValueSet() ? QString::number(_value) : QString();
}

///////////////////////////////////////////////////////////////////////////////

GzipCsv::ColDouble::ColDouble(GzipCsv * container, const std::string & name,
	const std::string &format)
	: ColBase(container, name), _format(format)
{}

QString GzipCsv::ColDouble::formatValue() const
{
	if (!isValueSet()) {
		return QString();
	}
	if (!_format.empty()) {
		return QString::asprintf(_format.c_str(), _value);
	}
	return QString::fromStdString(boost::lexical_cast<std::string>(_value));
}

///////////////////////////////////////////////////////////////////////////////

GzipCsv::ColStr::ColStr(GzipCsv *container, const std::string &name)
	: ColBase(container, name)
{}

QString GzipCsv::ColStr::formatValue() const
{
	return isValueSet() ? QString::fromStdString(_value) : QString();
}

///////////////////////////////////////////////////////////////////////////////

GzipCsv::ColBool::ColBool(GzipCsv *container, const std::string &name,
	const std::vector<std::string> &tf)
	: ColBase(container, name)
{
	assert(tf.size() == 2);
	for (const auto &s : tf) {
		_tf.push_back(QString::fromStdString(s));
	}
}

QString GzipCsv::ColBool::formatValue() const
{
	return isValueSet() ? _tf[_value ? 0 : 1] : QString();
}

///////////////////////////////////////////////////////////////////////////////

GzipCsv::ColEnum::ColEnum(GzipCsv *container, const std::string &name,
	const std::map<int, std::string> &items, const std::string &defName)
	: ColBase(container, name), _defName(QString::fromStdString(defName))
{
	for (const auto &kvp: items) {
		_items[kvp.first] = QString::fromStdString(kvp.second);
	}
}

QString GzipCsv::ColEnum::formatValue() const
{
	if (!isValueSet()) {
		return QString();
	}
	const auto &it = _items.find(_value);
	return (it == _items.end()) ? (_defName + QString::asprintf(" (%d)", _value)) : it->second;
}
