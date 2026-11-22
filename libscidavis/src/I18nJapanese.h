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
    // Translation used only when tr() is called from the given context (class name). Wins
    // over the context-free dictionary and registerExtraTranslation() for that context.
    static void registerContextTranslation(const QString &context, const QString &sourceEn,
                                           const QString &targetJa);
    static QString lookupJapaneseInContext(const QString &context, const QString &sourceEn);
    // Applies the glossary rules (see scripts/check_ja_glossary.py) to a translation.
    static QString normalizeGlossary(const QString &sourceEn, const QString &ja);
    static int builtinDictionarySize();
    static int contextDictionarySize();
    static QString lookupJapanese(const QString &sourceEn);
    static int dictionarySize();
};

#endif
