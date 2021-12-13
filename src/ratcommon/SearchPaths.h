// Copyright (C) 2017 RKF Engineering Solutions, LLC

#ifndef SEARCH_PATHS_H
#define SEARCH_PATHS_H

#include <QString>

/** Utilities to locate configuration files in the file system.
 * The SearchPaths::init() function is not thread safe, since it writes global
 * data. The other functions are thread safe since they are read-only.
 */
namespace SearchPaths{
    /** Set up the search path for config and data prefix.
     * On windows, this uses the operating system standard paths for data.
     * If defined, the environment variable LOCALAPPDATA will also be used
     * for data search override on windows.
     *
     * On unix, this uses recommendations defined by:
     * http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
     *
     * @param pathSuffix This suffix is appended to each of the base search
     * paths.
     * @return True if the initialization succeeded.
     */
    bool init(const QString &pathSuffix=QString());
    
    /** Get the full ordered list of possible file paths.
     * @param prefix The search prefix to use with QDir::searchPaths().
     * @param fileName The file path to append to each search path.
     * @return All paths of that prefix type.
     */
    QStringList allPaths(const QString &prefix, const QString &fileName);

    /** Get the first writable absolute file name in a search prefix.
     * @param prefix The search prefix to use with QDir::searchPaths().
     * @param fileName The file path to append to each search path.
     * @return The absolute path of the first writable search path, or
     * an empty string if no search path is writable.
     */
    QString forWriting(const QString &prefix, const QString &fileName);

    /** Get the first existing absolute file name in a search prefix.
     * @param prefix The search prefix to use with QDir::searchPaths().
     * @param fileName The file path to append to each search path.
     * @param required If true, then the lookup is manditory and a failure
     * will throw an exception.
     * @return The absolute path of the first search path which contains the
     * desired file name, or an empty string if no path has the file.
     * @throw std::runtime_error If the path is required but not present.
     */
    QString forReading(const QString &prefix, const QString &fileName, bool required=false);
}

#endif /* SEARCH_PATHS_H */
