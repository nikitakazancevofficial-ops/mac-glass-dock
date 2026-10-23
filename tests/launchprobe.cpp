#include <QFile>

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    QFile marker(QString::fromLocal8Bit(argv[1]));
    return marker.open(QIODevice::WriteOnly) && marker.write("launched\n") > 0 ? 0 : 1;
}
