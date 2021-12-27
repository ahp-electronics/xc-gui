#ifndef ELEMENTAL_H
#define ELEMENTAL_H

#include "types.h"
#include <QFileInfo>
#include <QObject>
#include <QDir>
#include <QMap>
#include <vlbi.h>
#include "threads.h"
#include "types.h"

class Elemental : public QObject
{
    Q_OBJECT
private:
    int matches;
    Thread *scanThread;
    dsp_stream_p stream;
    QStringList catalogPath {QStringList(VLBI_CATALOG_PATH)};
    int sample {5};

public:
    explicit Elemental(QObject *parent = nullptr);
    ~Elemental();

    static dsp_stream_p reference;
    static QList <dsp_stream_p> elements;

    inline void setCatalogPath(QString path) { catalogPath.append(path); }
    void setBuffer(double * buf, int len);
    inline void setSampleSize(int value) { sample = value; }
    inline int getSampleSize() { return sample; }
    inline dsp_stream_p getStream() { return stream; }
    void scan();
    QStringList getElementNames();
    dsp_align_info *stats(QString name);
    static void loadCatalog(QString path = VLBI_CATALOG_PATH);
    static void unloadCatalog();
    inline QList <dsp_stream_p> getCatalog() { return Elemental::elements; }

signals:

};


#endif // ELEMENTAL_H
