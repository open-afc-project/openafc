/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */

/** @file
 * GZIPped CSV writer that separate field definition from their formatting and
 * ordering.
 * 
 * Each CSV format (set of columns and their types) has its own class:
 * #include "GzipCsv.h"
 * //// Declaration
 * class FooCsv : public GzipCsv {
 * public:
 *     FooCsv(const std::string &filename);
 * 
 *     // Declaring CSV columns - in filing order
 *     ColDouble bar;
 *     ColStr baz;
 * 
 *     FooCsv(const std::string &filename)
 *         : GzipCsv(filename),
 *         // Defining names and formattings of columns
 *         bar(this, "BAR", "12f"), baz(this, "BAZ")
 *     {}
 * };
 * ...
 * //// Use
 * // Initialization. Empty name means nothing will be written
 * fooCsv = FooCsv(file_name);
 * ...
 * // Optional check if writer was activated (initialized with nonempty name)
 * if (fooCsv) {
 *     // Setting column values in any order
 *     fooCsv.baz = "a string";
 *     fooCsv.bar = 57.179;
 *     // Writing row with values previously set (and resetting values)
 *     fooCsv.writeRow();
 * }
 */

#ifndef GZIP_CSV_H
#define GZIP_CSV_H

#include "CsvWriter.h"
#include "GzipStream.h"
#include <map>
#include <memory>
#include <boost/core/noncopyable.hpp>
#include <QFile>
#include <QString>
#include <string>
#include <vector>

/** Base class for CSV GZIP writers.
 * Defines initialization/writing logic, but does not define specific columns -
 * they should be defined in derived classes. Also servers as a namespace for
 * column classes (to make type names shorter in declaration of derived classes)
 */
class GzipCsv : private boost::noncopyable {
public:

	///////////////////////////////////////////////////////////////////////////
	// CLASSES FOR COLUMNS
	///////////////////////////////////////////////////////////////////////////

	/** Abstract base class for columns.
	 * Defines general management, but leavse value storing and formatting to
	 * derived classes
	 */
	class ColBase : private boost::noncopyable {
	public:
		/** Default virtual destructor */
		virtual ~ColBase() = default;

		/** True if column value was set */
		bool isValueSet() const {return _valueSet;}

		/** Mark column value as not set */
		void resetValue() {_valueSet = false;}

	protected:

		/** Constructor
		 * @param container GzipCsv-derived object that stores column values
		 * @param name Column name
		 */
		ColBase(GzipCsv *container, const std::string &name);

		/** Mark column as set (assigned value) */
		void markSet() {_valueSet = true;}

		/** Column name */
		const QString &name() const {return _name;}

		/** Returns column value formatted for putting to CSV. "" if not set */
		virtual QString formatValue() const = 0;

		/** Raise exception if value not set */
		void checkSet() const;
	
	private:
		friend class GzipCsv;

		// INSTANCE DATA

		const QString _name;		/*!< Column name */
		bool _valueSet = false;		/*!< Column value set */
	};

	///////////////////////////////////////////////////////////////////////////

	/** Class for integer columns */
	class ColInt : public ColBase {
	public:
		/** Constructor
		 * @param container GzipCsv-derived object that stores column values
		 * @param name Column name
		 */
		ColInt(GzipCsv *container, const std::string &name);

		/** Setting field value */
		ColInt &operator = (int value) {markSet(); _value = value; return *this;}

		/** Returning field value (assert if not set) */
		int value() const {checkSet(); return _value;}

	protected:

		/** Returns column value formatted for putting to CSV. "" if not set */
		virtual QString formatValue() const;

	private:

		// INSTANCE DATA

		int _value;		/*!< Column value (for current row) */
	};

	///////////////////////////////////////////////////////////////////////////

	/** Class for floating point columns */
	class ColDouble : public ColBase {
	public:
		/** Constructor
		 * @param container GzipCsv-derived object that stores column values
		 * @param name Column name
		 * @pasram format Printf-style format. Empty to show at maximum precision
		 */
		ColDouble(GzipCsv *container, const std::string &name,
			const std::string &format = "");

		/** Setting field value */
		ColDouble &operator = (double value) {markSet(); _value = value; return *this;}

		/** Returning field value (assert if not set) */
		double value() const {checkSet(); return _value;}

	protected:

		/** Returns column value formatted for putting to CSV. "" if not set */
		virtual QString formatValue() const;

	private:

		// INSTANCE DATA

		double _value;			/*!< Column value (for current row) */
		std::string _format;	/*!< Printf-style format. Empty for maximum precision */
	};

	///////////////////////////////////////////////////////////////////////////

	/** Class for string columns */
	class ColStr : public ColBase {
	public:
		/** Constructor
		 * @param container GzipCsv-derived object that stores column values
		 * @param name Column name
		 */
		ColStr(GzipCsv *container, const std::string &name);

		/** Setting field value */
		ColStr &operator = (const std::string &value) {markSet(); _value = value; return *this;}

		/** Returning field value (assert if not set) */
		const std::string &value() const {checkSet(); return _value;}

	protected:

		/** Returns column value formatted for putting to CSV. "" if not set */
		virtual QString formatValue() const;

	private:

		// INSTANCE DATA

		std::string _value;		/*!< Column value (for current row) */
	};

	///////////////////////////////////////////////////////////////////////////

	/** Class for bool columns */
	class ColBool : public ColBase {
	public:
		/** Constructor
		 * @param container GzipCsv-derived object that stores column values
		 * @param name Column name
		 * @param tf Column values for true and false
		 */
		ColBool(GzipCsv *container, const std::string &name,
			const std::vector<std::string> &tf = {"True", "False"});

		/** Setting field value */
		ColBool &operator = (bool value) {markSet(); _value = value; return *this;}

		/** Returning field value (assert if not set) */
		bool value() const {checkSet(); return _value;}

	protected:

		/** Returns column value formatted for putting to CSV */
		virtual QString formatValue() const;

	private:

		// INSTANCE DATA

		bool _value;				/*!< Column value (for current row) */
		std::vector<QString> _tf;	/*!< Column values for true and false */
	};

	///////////////////////////////////////////////////////////////////////////

	/** Class for enum columns */
	class ColEnum : public ColBase {
	public:

		/** Constructor
		 * @param container GzipCsv-derived object that stores column values
		 * @param name Column name
		 * @param items Enum item descriptors
		 * @param defName Name for unknown item
		 */
		ColEnum(GzipCsv *container, const std::string &name,
			const std::map<int, std::string> &items,
			const std::string &defName = "Unknown");

		/** Setting field value */
		ColEnum &operator = (int value) {markSet(); _value = value; return *this;}

		/** Returning field value (assert if not set) */
		int value() const {checkSet(); return _value;}

	protected:

		/** Returns column value formatted for putting to CSV */
		virtual QString formatValue() const;

	private:

		// INSTANCE DATA

		int _value;						/*!< Column value (for current row) */
		std::map<int, QString> _items;	/*!< Item descriptors */
		QString _defName;				/*<! Name for unknown items */
	};

	///////////////////////////////////////////////////////////////////////////

	/** Constructor
	 * @param filename Name of CSV GZIP file (expected to have .csv.gz extension).
	 *	Empty if writer should not be activated
	 */
	GzipCsv(const std::string &filename);

	/** Default virtual destructor */
	virtual ~GzipCsv() = default;

	/** True if writer was active (initialized with nonempty file name */
	operator bool() const {return (bool)_fileWriter;}

	/** Marks all columns as not set */
	void clearRow();

	/** Writes row with currently set values, marks all columns as not set */
	void writeRow();

private:
	friend class ColBase;

	/** Append reference to column to vector of columns */
	void addColumn(ColBase *column);


	// INSTANCE DATA

	/** True if heading row have been written */
	bool _headingWritten = false;

	/** File writer, used by gzip writer */
	std::unique_ptr<QFile> _fileWriter;

	/** GZIP writer used by CSV writer */
	std::unique_ptr<GzipStream> _gzipWriter;

	/** CSV writer */
	std::unique_ptr<CsvWriter> _csvWriter;

	/** Vector of columns */
	std::vector<ColBase *> _columns;
};
#endif /* GZIP_CSV_H */
