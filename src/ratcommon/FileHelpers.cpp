// Copyright (C) 2017 RKF Engineering Solutions, LLC

#include "FileHelpers.h"
#include "rkflogging/ErrStream.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

std::unique_ptr<QFile> FileHelpers::open(const QString &name, QIODevice::OpenMode mode){
    std::unique_ptr<QFile> file(new QFile(name));
    if(!file->open(mode)){
        throw Error(
            QString("Error opening file \"%1\" in mode %2: %3")
                .arg(name).arg(mode).arg(file->errorString())
        );
    }
    return file;
}

std::unique_ptr<QFile> FileHelpers::openWithParents(const QString &name, QIODevice::OpenMode mode){
    ensureParents(QFileInfo(name));
    return open(name, mode);
}

void FileHelpers::ensureParents(const QFileInfo &fileName){
    if(fileName.exists()){
        return;
    }

    ensureExists(fileName.absoluteDir());
}

void FileHelpers::ensureExists(const QDir &path){
    if(path.exists()){
        return;
    }

    const QString fullPath = path.absolutePath();
    if(!QDir::root().mkpath(fullPath)){
        throw Error(QString("Failed to create path \"%1\"").arg(fullPath));
    }
}

void FileHelpers::remove(const QFileInfo &filePath) {
    QFile file(filePath.absoluteFilePath());
    if(!file.remove()){
        throw Error(ErrStream() << "Failed to remove \"" << file.fileName() << "\": " << file.errorString());
    }

}

void FileHelpers::removeTree(const QDir &root){
    QDir dir(root);
    for (const auto &dirInfo : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        removeTree(dirInfo.absoluteFilePath());
    }
    for (const auto &fileInfo : dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System)) {
        FileHelpers::remove(fileInfo);
    }

    if(!dir.rmdir(dir.absolutePath())){
        throw Error(ErrStream() << "Failed to remove directory \"" << dir.absolutePath() << "\"");
    }
}
