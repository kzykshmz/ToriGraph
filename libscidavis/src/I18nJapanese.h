#ifndef I18N_JAPANESE_H
#define I18N_JAPANESE_H

#include <QTranslator>
#include <QString>

class ToriGraphFullJapaneseTranslator : public QTranslator
{
public:
    explicit ToriGraphFullJapaneseTranslator(QObject *parent = nullptr);
    bool isEmpty() const override;
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;
    static void registerExtraTranslation(const QString &sourceEn, const QString &targetJa);
    static QString lookupJapanese(const QString &sourceEn);
    static int dictionarySize();
};

#endif
