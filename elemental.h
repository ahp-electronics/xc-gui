#ifndef ELEMENTAL_H
#define ELEMENTAL_H

#include <QFileInfo>
#include <QObject>
#include <QDir>
#include <QMap>
#include "types.h"

class Elemental : public QObject
{
    Q_OBJECT
    private:
        int matches;
        Thread *scanThread;
        dsp_stream_p stream;
        bool success { false };
        double offset { 0.0 };
        double scale { 1.0 };
        int sample {5};
        int maxDots {10};
        int decimals { 0 };
        int minScore { 50 };

    public:
        explicit Elemental(QObject *parent = nullptr);
        ~Elemental();

        void run();
        void finish(bool done = false, double ofs = 0.0, double sc = 1.0);
        static dsp_stream_p reference;
        static QList <dsp_stream_p> elements;

        inline void setMaxDots(int value) { maxDots = value; }
        inline void setDecimals(int value) { decimals = value; }
        inline void setMinScore(int value) { minScore = value; }
        inline int getMaxDots() { return maxDots; }
        inline int getDecimals() { return decimals; }
        inline int getMinScore() { return minScore; }
        inline double getOffset() { return offset; }
        inline double getScale() { return scale; }
        void setBuffer(double * buf, int len);
        void setMagnitude(double * buf, int len);
        void setPhase(double * buf, int len);
        void setReal(double * buf, int len);
        void setImaginary(double * buf, int len);
        void idft();
        void dft(int depth);
        inline void setSampleSize(int value) { sample = value; }
        inline int getSampleSize() { return sample; }
        inline dsp_stream_p getStream() { return stream; }
        void scan();
        QStringList getElementNames();
        dsp_align_info *stats(QString name);
        static void loadCatalog();
        static void unloadCatalog();
        inline QList <dsp_stream_p> getCatalog() { return Elemental::elements; }

    signals:
        void scanFinished(bool, double , double );
};


#endif // ELEMENTAL_H
