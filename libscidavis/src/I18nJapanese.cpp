/***************************************************************************
    File                 : I18nJapanese.cpp
    Project              : ToriGraph
    Description          : Exhaustive Japanese GUI Translator (OriginPro style)
 ***************************************************************************/
#include "I18nJapanese.h"

#include <QChar>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QStringRef>
#include <QVector>
#include <cstring>

// Q_INIT_RESOURCE must be used outside of any namespace. The dictionary resource is part of
// the static libscidavis library, so it has to be registered explicitly before first use.
static void initJapaneseDictionaryResource()
{
    Q_INIT_RESOURCE(ja_dictionary);
}

namespace {

// The dictionary itself lives in i18n/ja_dictionary.tsv (UTF-8, human readable) and is
// compiled in as a Qt resource. Format and the glossary check: scripts/check_ja_glossary.py.
//   source<TAB>translation[<TAB>context]
// Rows with a context (the tr() class name) are only used for that context and take
// precedence over the context-free default (two-stage lookup, see translate()).

static QMutex s_dictMutex;
static bool s_initialized = false;
static QHash<QString, QString> s_mainDict;
static QHash<QString, QString> s_lowerDict;
static QHash<QString, QString> s_extraDict;
static QHash<QString, QString> s_extraLowerDict;
static QHash<QString, QString> s_contextDict; // key: context + '\x1f' + source
static QSet<QString> s_guardExactCs;
static QSet<QString> s_guardCaseInsensitive;
static QSet<QString> s_canonicalMainKeys;
static int s_builtinEntryCount = 0;

static QString contextKey(const QString &context, const QString &source)
{
    return context + QChar(0x1f) + source;
}

static QString unescapeTsvField(const QString &field)
{
    if (!field.contains(QLatin1Char('\\')))
        return field;
    QString out;
    out.reserve(field.size());
    for (int i = 0; i < field.size(); ++i) {
        const QChar c = field.at(i);
        if (c == QLatin1Char('\\') && i + 1 < field.size()) {
            const QChar n = field.at(i + 1);
            ++i;
            if (n == QLatin1Char('n'))
                out.append(QLatin1Char('\n'));
            else if (n == QLatin1Char('t'))
                out.append(QLatin1Char('\t'));
            else if (n == QLatin1Char('r'))
                out.append(QLatin1Char('\r'));
            else if (n == QLatin1Char('s'))
                out.append(QLatin1Char(' '));
            else if (n == QLatin1Char('\\'))
                out.append(QLatin1Char('\\'));
            else {
                out.append(QLatin1Char('\\'));
                out.append(n);
            }
            continue;
        }
        out.append(c);
    }
    return out;
}

static void loadBuiltinDictionaryLocked()
{
    initJapaneseDictionaryResource();
    QFile f(QStringLiteral(":/i18n/ja_dictionary.tsv"));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("ToriGraph: built-in Japanese dictionary resource is missing");
        return;
    }
    const QString text = QString::fromUtf8(f.readAll());
    const QVector<QStringRef> lines = text.splitRef(QLatin1Char('\n'));
    s_mainDict.reserve(lines.size() + 128);
    s_lowerDict.reserve(lines.size() + 128);
    for (QStringRef line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QVector<QStringRef> cols = line.split(QLatin1Char('\t'));
        if (cols.size() < 2)
            continue;
        const QString en = unescapeTsvField(cols.at(0).toString());
        const QString ja = unescapeTsvField(cols.at(1).toString());
        if (en.isEmpty() || ja.isEmpty())
            continue;
        if (cols.size() >= 3 && !cols.at(2).isEmpty()) {
            s_contextDict.insert(contextKey(cols.at(2).toString(), en), ja);
            continue;
        }
        s_mainDict.insert(en, ja);
        ++s_builtinEntryCount;
        const QString low = en.toLower();
        if (!s_lowerDict.contains(low))
            s_lowerDict.insert(low, ja);
    }
}

// Glossary normalisation for translations registered from code (registerExtraTranslation).
// The same rules are enforced on ja_dictionary.tsv by scripts/check_ja_glossary.py; keep
// both lists in sync.
struct GlossaryRule {
    QRegularExpression pattern;
    QString replacement;
    QRegularExpression enOnly;
    QRegularExpression enExcept;
};

static const QVector<GlossaryRule> &glossaryRules()
{
    static const QVector<GlossaryRule> rules = [] {
        QVector<GlossaryRule> r;
        const auto ci = QRegularExpression::CaseInsensitiveOption;
        auto add = [&r, ci](const char *pat, const char *rep, const char *only = nullptr,
                            const char *except = nullptr) {
            GlossaryRule g;
            g.pattern = QRegularExpression(QString::fromUtf8(pat));
            g.replacement = QString::fromUtf8(rep);
            if (only)
                g.enOnly = QRegularExpression(QString::fromUtf8(only), ci);
            if (except)
                g.enExcept = QRegularExpression(QString::fromUtf8(except), ci);
            r.append(g);
        };
        add("\xEF\xBC\x88", "("); // full-width (
        add("\xEF\xBC\x89", ")"); // full-width )
        add("\xEF\xBC\x9A", ":"); // full-width :
        add("OK \\(決定\\)", "OK");
        add("レイヤ(?!ー)", "レイヤー");
        add("目盛(?!り)", "目盛り");
        add("誤差棒|誤差バー", "エラーバー");
        add("フィッティング", "フィット");
        add("スムージング \\(平滑化\\) ?", "平滑化");
        add("スムージング", "平滑化");
        add("インスペクター", "インスペクタ");
        add("マトリッ?クス", "行列");
        add("エキスポート", "エクスポート");
        add("軸ラベル|軸題(?!名)", "軸タイトル", nullptr, "Category");
        add("テーブル", "ワークシート", nullptr,
            "TeX|ASCII|caption|Table labe?l?s|^Table1$|element");
        add("(?<!特殊)記号", "シンボル",
            "Symbol (Shape|Size|Edge|Fill|Skip|Color|Opacity|&)|Symbol Shapes|"
            "Line ?\\+ ?Sym|No Symbol|Colors && Symbols|symbols, frame|symbol styles|"
            "Style Scheme");
        add("\\s*&&\\s*", "・");
        add("(?<!不)透明度", "不透明度", "Opacity");
        add("プレゼン発表スライド|プレゼンスライド", "発表スライド");
        add("物理量 / 単位 \\(ISIJ形式\\)", "物理量 / 単位 (ISIJ)");
        add("ロングネーム", "長い名前");
        add("プロット属性", "列の役割");
        add("バンド構造 \\(Band Structure\\)", "バンド構造");
        add("縦横比を固定 \\(Lock Aspect Ratio\\)", "縦横比を固定");
        return r;
    }();
    return rules;
}

static QString stripEchoImpl(const QString &sourceEn, const QString &ja)
{
    static const QSet<QString> kEchoStripEn = {
        QStringLiteral("black"), QStringLiteral("blue"), QStringLiteral("cyan"),
        QStringLiteral("dark cyan"), QStringLiteral("dark gray"), QStringLiteral("dark yellow"),
        QStringLiteral("gray"), QStringLiteral("green"), QStringLiteral("light cyan"),
        QStringLiteral("light gray"), QStringLiteral("light magenta"),
        QStringLiteral("light yellow"), QStringLiteral("magenta"), QStringLiteral("navy"),
        QStringLiteral("olive"), QStringLiteral("orange"), QStringLiteral("pink"),
        QStringLiteral("purple"), QStringLiteral("red"), QStringLiteral("royal"),
        QStringLiteral("violet"), QStringLiteral("white"), QStringLiteral("wine"),
        QStringLiteral("yellow"), QStringLiteral("linear"), QStringLiteral("left"),
        QStringLiteral("right"), QStringLiteral("top"), QStringLiteral("bottom"),
        QStringLiteral("flat"), QStringLiteral("round"), QStringLiteral("square"),
        QStringLiteral("solid"), QStringLiteral("bevel"), QStringLiteral("miter"),
        QStringLiteral("bdiagonal"), QStringLiteral("fdiagonal"), QStringLiteral("cross"),
        QStringLiteral("diagcross"), QStringLiteral("diamond"), QStringLiteral("rectangle"),
        QStringLiteral("cone"), QStringLiteral("slope"), QStringLiteral("intercept"),
        QStringLiteral("real"), QStringLiteral("imaginary"), QStringLiteral("median"),
        QStringLiteral("cross hair"), QStringLiteral("intersect"),
        QStringLiteral("upper envelope"), QStringLiteral("lower envelope"),
        QStringLiteral("both (upper & lower)"), QStringLiteral("before column:"),
        QStringLiteral("after column:"), QStringLiteral("peak analyzer goal"),
        QStringLiteral("peak analyzer goal:"), QStringLiteral("dissimilarity height"),
        QStringLiteral("box plot"), QStringLiteral("pareto chart"), QStringLiteral("ternary plot"),
        QStringLiteral("autoexec"), QStringLiteral("average")
    };
    QString key = sourceEn;
    key.remove(QLatin1Char('&'));
    key = key.trimmed().toLower();
    if (!kEchoStripEn.contains(key))
        return ja;
    static const QRegularExpression rxEcho(QStringLiteral(
            "\\s?\\((?:[A-Z][A-Za-z\\- ]*|Both|Before|After|Goal|Height|Pareto|Ternary)\\)"));
    QString out = ja;
    const QRegularExpressionMatch m = rxEcho.match(out);
    if (m.hasMatch())
        out.remove(m.capturedStart(), m.capturedLength());
    return out;
}

static QString stripEnMnemonic(const QString &s, QChar *outMnemonic, bool *outHadDoubleAmp)
{
    if (outMnemonic)
        *outMnemonic = QChar();
    if (outHadDoubleAmp)
        *outHadDoubleAmp = false;

    QString out;
    out.reserve(s.size());
    const int n = s.size();
    for (int i = 0; i < n; ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('&')) {
            if (i + 1 < n && s.at(i + 1) == QLatin1Char('&')) {
                out.append(QLatin1Char('&'));
                if (outHadDoubleAmp)
                    *outHadDoubleAmp = true;
                ++i;
                continue;
            }
            if (i + 1 < n && s.at(i + 1).isLetterOrNumber() && s.at(i + 1).unicode() < 128) {
                if (outMnemonic && outMnemonic->isNull())
                    *outMnemonic = s.at(i + 1).toUpper();
                continue;
            }
        }
        out.append(ch);
    }
    return out;
}

static bool hasAsciiLetter(const QString &s)
{
    for (int i = 0; i < s.size(); ++i) {
        const ushort u = s.at(i).unicode();
        if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))
            return true;
    }
    return false;
}

static bool isKeyboardShortcutString(const QString &st)
{
    const QString low = st.toLower();
    if (low.startsWith(QLatin1String("ctrl+")) || low.startsWith(QLatin1String("alt+"))
        || low.startsWith(QLatin1String("shift+")) || low.startsWith(QLatin1String("meta+"))) {
        return true;
    }
    if (low.size() >= 2 && low.size() <= 3 && low.at(0) == QLatin1Char('f')) {
        bool ok = false;
        const int fnum = low.mid(1).toInt(&ok);
        if (ok && fnum >= 1 && fnum <= 12)
            return true;
    }
    return false;
}

static void ensureInitializedLocked()
{
    if (s_initialized)
        return;

    loadBuiltinDictionaryLocked();

    s_guardCaseInsensitive.insert(QStringLiteral("arial"));
    s_guardCaseInsensitive.insert(QStringLiteral("torigraph"));
    s_guardCaseInsensitive.insert(QStringLiteral("torigraph xx.xx.xx-betaxx"));

    static const char *const kProtectedTokens[] = {
        "user1", "Table1", "MergedGraph", "SD", "Del", "Hz", "F", "s",
        "R =", "R2", "R^2", "X=", "X=0", "Y=", "Y=0",
        "f(x)=", "f(x,y)=", "x =", "y =",
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
        "sinh", "cosh", "tanh", "acosh", "asinh", "atanh",
        "exp", "ln", "log", "log10", "log2", "sqrt", "abs", "sign", "rint",
        "ceil", "floor", "pow", "mod", "erf", "erfc", "erfz", "erfq",
        "gamma", "gammaln", "hazard", "beta", "w0", "wm1",
        "bessel_j0", "bessel_j1", "bessel_jn", "bessel_jn_zero",
        "bessel_y0", "bessel_y1", "bessel_yn",
        "avg", "min", "max", "sum", "pi", "e",
        "x", "y", "z", "t", "x1", "y1", "x2", "y2", "xc", "y0", "w", "a",
        "X", "Y", "Z", "A"
    };
    const int nProt = static_cast<int>(sizeof(kProtectedTokens) / sizeof(kProtectedTokens[0]));
    for (int i = 0; i < nProt; ++i)
        s_guardExactCs.insert(QString::fromLatin1(kProtectedTokens[i]));

    static const char *const kCanonicalKeysUtf8[] = {
        "Panel View",
        "28pt Slide",
        "Square 70%",
        "Merge Graphs",
        "Merge Multiple Open Graph Windows into Multi-Panel Figure",
        "Clone Active Graph Template Across Multiple Worksheets",
        "Export Graph",
        "Export Graph to PNG / TIFF / SVG / EMF",
        "Export Vector PDF Document",
        "Run Python Script",
        "Classic Toolbars",
        "Show or Hide Legacy Classic Icon Toolbars",
        "Side-by-Side Split View (40% Worksheet / 60% Graph)",
        "Enter Full-Canvas Focus HUD Mode (F11)",
        "[\302\261xErr]",
        "[\302\261yErr]",
        "\302\261xErr",
        "\302\261yErr",
        "More column plot roles (\302\261xErr, Z, Label, Disregard)",
        "Graphs (2D/3D)",
        "Matrices",
        "Tables (Workbooks)",
        "All Windows",
        "Notes / Scripts",
        "Fit",
        "Axis",
        "Free",
        "Paste",
        "Paste (Ctrl+V)",
        "Line + Symbol",
        "Add Error Bars...",
        "Export JSON...",
        "Yes",
        "No",
        "Long Name",
        "Long Name:",
        "Long Name: %1",
        "Export Python",
        "Export Python Script",
        "Export Active Graph to Python Script...",
        "Export Reproducible &Python Script...",
        "Export Reproducible Python Script...",
        "Export Reproducible Python Script \342\200\224 %1"
    };
    const int nCanon =
            static_cast<int>(sizeof(kCanonicalKeysUtf8) / sizeof(kCanonicalKeysUtf8[0]));
    for (int i = 0; i < nCanon; ++i)
        s_canonicalMainKeys.insert(QString::fromUtf8(kCanonicalKeysUtf8[i]));

    // TeXTableSettingsWidget.cpp has a historical source literal typo "Table labes";
    // keep its context translation in code so ja_dictionary.tsv stays typo-free.
    s_contextDict.insert(contextKey(QStringLiteral("TeXTableSettingsWidget"),
                                    QStringLiteral("Table labes")),
                         QString::fromUtf8("テーブルラベル (参照キー)"));

    s_initialized = true;
}

static bool isProtectedTokenLocked(const QString &trimmed)
{
    if (trimmed.isEmpty())
        return true;
    if (s_guardCaseInsensitive.contains(trimmed.toLower()))
        return true;
    if (s_guardExactCs.contains(trimmed))
        return true;
    if (isKeyboardShortcutString(trimmed))
        return true;
    if (!hasAsciiLetter(trimmed))
        return true;
    return false;
}

static bool endsWithPunctuation(const QString &ja)
{
    return ja.endsWith(QLatin1String("..."))
           || ja.endsWith(QChar(0x2026)) // '…'
           || ja.endsWith(QLatin1Char(':'))
           || ja.endsWith(QChar(0xFF1A)); // '：'
}

static QString attachMnemonic(const QString &ja, QChar mnem)
{
    if (mnem.isNull())
        return ja;
    if (ja.contains(QLatin1String("(&"))
        || ja.contains(QLatin1Char('&') + QString(mnem), Qt::CaseInsensitive)) {
        return ja;
    }

    const QString tag = QStringLiteral("(&%1)").arg(mnem);
    if (ja.endsWith(QLatin1String(" >>")))
        return ja.left(ja.size() - 3) + tag + QStringLiteral(" >>");
    if (ja.endsWith(QLatin1String("...")))
        return ja.left(ja.size() - 3) + tag + QStringLiteral("...");
    if (ja.endsWith(QChar(0x2026)))
        return ja.left(ja.size() - 1) + tag + QChar(0x2026);
    if (ja.endsWith(QChar(0xFF1A)))
        return ja.left(ja.size() - 1) + tag + QChar(0xFF1A);
    if (ja.endsWith(QLatin1Char(':')))
        return ja.left(ja.size() - 1) + tag + QLatin1Char(':');
    return ja + tag;
}

static QString escapeStandaloneAmpersands(const QString &ja)
{
    QString out;
    out.reserve(ja.size() + 4);
    for (int i = 0; i < ja.size(); ++i) {
        if (ja.at(i) == QLatin1Char('&')) {
            if (i + 1 < ja.size() && ja.at(i + 1) == QLatin1Char('&')) {
                out.append(QLatin1String("&&"));
                ++i;
                continue;
            }
            // Keep (&X) mnemonic intact
            if (i > 0 && ja.at(i - 1) == QLatin1Char('(') && i + 2 < ja.size()
                && ja.at(i + 1).isLetterOrNumber() && ja.at(i + 2) == QLatin1Char(')')) {
                out.append(QLatin1Char('&'));
                continue;
            }
            out.append(QLatin1String("&&"));
            continue;
        }
        out.append(ja.at(i));
    }
    return out;
}

static QString lookupDictExactLocked(const QString &key)
{
    auto itExtra = s_extraDict.constFind(key);
    if (itExtra != s_extraDict.constEnd())
        return itExtra.value();
    auto itMain = s_mainDict.constFind(key);
    if (itMain != s_mainDict.constEnd())
        return itMain.value();
    return QString();
}

static QString lookupDictLowerLocked(const QString &lowerKey)
{
    auto itExtra = s_extraLowerDict.constFind(lowerKey);
    if (itExtra != s_extraLowerDict.constEnd())
        return itExtra.value();
    auto itMain = s_lowerDict.constFind(lowerKey);
    if (itMain != s_lowerDict.constEnd())
        return itMain.value();
    return QString();
}

static bool hasNonAsciiChar(const QString &s)
{
    for (int i = 0; i < s.size(); ++i) {
        if (s.at(i).unicode() >= 0x3000)
            return true;
    }
    return false;
}

static QString lookupCoreLocked(const QString &s, bool allowCaseInsensitive)
{
    QString direct = lookupDictExactLocked(s);
    if (!direct.isEmpty())
        return direct;

    QString leadArrows;
    QString trailArrows;
    QString work = s;
    if (work.startsWith(QLatin1String("<< "))) {
        leadArrows = QStringLiteral("<< ");
        work = work.mid(3);
    }
    if (work.endsWith(QLatin1String(" >>"))) {
        trailArrows = QStringLiteral(" >>");
        work.chop(3);
    }

    QString reattachSuffix;
    if (work.endsWith(QLatin1String("..."))) {
        reattachSuffix = QStringLiteral("...");
        work.chop(3);
        work = work.trimmed();
    } else if (work.endsWith(QChar(0x2026))) {
        reattachSuffix = QStringLiteral("...");
        work.chop(1);
        work = work.trimmed();
    } else if (work.endsWith(QLatin1String(" :"))) {
        reattachSuffix = QStringLiteral(":"); // half-width colon (glossary)
        work.chop(2);
        work = work.trimmed();
    } else if (work.endsWith(QLatin1Char(':'))) {
        reattachSuffix = QStringLiteral(":"); // half-width colon (glossary)
        work.chop(1);
        work = work.trimmed();
    }

    if (!work.isEmpty()) {
        QString foundExact = lookupDictExactLocked(work);
        if (!foundExact.isEmpty()) {
            if (!reattachSuffix.isEmpty() && !endsWithPunctuation(foundExact))
                foundExact += reattachSuffix;
            return leadArrows + foundExact + trailArrows;
        }
    }

    if (!allowCaseInsensitive)
        return QString();

    const QString lowFull = s.toLower();
    if (!s_guardCaseInsensitive.contains(lowFull) && !s_guardExactCs.contains(s)
        && !s_guardExactCs.contains(lowFull)) {
        QString directLow = lookupDictLowerLocked(lowFull);
        if (!directLow.isEmpty())
            return directLow;
    }

    if (work.isEmpty())
        return QString();

    const QString lw = work.toLower();
    if (!s_guardCaseInsensitive.contains(lw) && !s_guardExactCs.contains(work)
        && !s_guardExactCs.contains(lw)) {
        QString foundLow = lookupDictLowerLocked(lw);
        if (!foundLow.isEmpty()) {
            if (!reattachSuffix.isEmpty() && !endsWithPunctuation(foundLow))
                foundLow += reattachSuffix;
            return leadArrows + foundLow + trailArrows;
        }
    }
    return QString();
}

static QString normalizeGlossaryImpl(const QString &sourceEn, const QString &ja)
{
    QString out = ja;
    for (const GlossaryRule &r : glossaryRules()) {
        if (!r.enOnly.pattern().isEmpty() && !r.enOnly.match(sourceEn).hasMatch())
            continue;
        if (!r.enExcept.pattern().isEmpty() && r.enExcept.match(sourceEn).hasMatch())
            continue;
        out.replace(r.pattern, r.replacement);
    }
    return stripEchoImpl(sourceEn, out);
}

// Context-specific lookup (first stage of the two-stage lookup). Tries the source as is,
// trimmed, and with the &-mnemonic removed (re-attaching it as "(&X)").
static QString lookupContextLocked(const QString &context, const QString &source)
{
    if (s_contextDict.isEmpty() || context.isEmpty())
        return QString();
    auto it = s_contextDict.constFind(contextKey(context, source));
    if (it != s_contextDict.constEnd())
        return it.value();

    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty())
        return QString();
    if (trimmed != source) {
        it = s_contextDict.constFind(contextKey(context, trimmed));
        if (it != s_contextDict.constEnd()) {
            const int lead = source.indexOf(trimmed);
            return source.left(lead) + it.value() + source.mid(lead + trimmed.size());
        }
    }

    QChar mnem;
    bool hadDouble = false;
    QString clean = stripEnMnemonic(trimmed, &mnem, &hadDouble);
    QString suffix;
    if (clean.endsWith(QLatin1String("..."))) {
        suffix = QStringLiteral("...");
        clean.chop(3);
    } else if (clean.endsWith(QLatin1Char(':'))) {
        suffix = QStringLiteral(":");
        clean.chop(1);
    }
    clean = clean.trimmed();
    if (clean == trimmed)
        return QString();
    it = s_contextDict.constFind(contextKey(context, clean));
    if (it == s_contextDict.constEnd())
        return QString();
    QString ja = it.value();
    if (!suffix.isEmpty() && !endsWithPunctuation(ja))
        ja += suffix;
    if (hadDouble)
        ja = escapeStandaloneAmpersands(ja);
    return attachMnemonic(ja, mnem);
}

} // namespace

ToriGraphFullJapaneseTranslator::ToriGraphFullJapaneseTranslator(QObject *parent)
    : QTranslator(parent)
{
    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();
}

bool ToriGraphFullJapaneseTranslator::isEmpty() const
{
    return dictionarySize() == 0;
}

QString ToriGraphFullJapaneseTranslator::translate(const char *context, const char *sourceText,
                                                   const char *disambiguation, int n) const
{
    if (!sourceText || sourceText[0] == '\0')
        return QString();

    // Qt / ConfigDialog locale self-name discovery probe
    if (disambiguation && std::strstr(disambiguation, "language of the translation file") != nullptr
        && std::strcmp(sourceText, "English") == 0) {
        return QString::fromUtf8("\346\227\245\346\234\254\350\252\236 (Japanese)");
    }

    // Two-stage lookup: the same English word can mean different things in different
    // dialogs ("Center" = alignment in LayerDialog, peak centre in MultiPeakFit), so a
    // translation registered for the tr() context wins over the context-free default.
    const QString source = QString::fromUtf8(sourceText);
    QString res;
    if (context && context[0] != '\0')
        res = lookupJapaneseInContext(QString::fromLatin1(context), source);
    if (res.isEmpty())
        res = lookupJapanese(source);
    if (!res.isEmpty() && n >= 0 && res.contains(QLatin1String("%n")))
        res.replace(QLatin1String("%n"), QString::number(n));
    return res;
}

void ToriGraphFullJapaneseTranslator::registerExtraTranslation(const QString &sourceEn,
                                                               const QString &targetJaIn)
{
    if (sourceEn.isEmpty() || targetJaIn.isEmpty())
        return;

    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();

    // Translations registered from code follow the same glossary as ja_dictionary.tsv
    // (レイヤー, 目盛り, half-width brackets, ...), see scripts/check_ja_glossary.py.
    const QString targetJa = normalizeGlossaryImpl(sourceEn, targetJaIn);

    const QString trimmed = sourceEn.trimmed();
    if (s_guardCaseInsensitive.contains(trimmed.toLower()))
        return;
    if (s_canonicalMainKeys.contains(sourceEn) || s_canonicalMainKeys.contains(trimmed))
        return;
    if (!hasNonAsciiChar(targetJa)
        && ((s_mainDict.contains(sourceEn) && hasNonAsciiChar(s_mainDict.value(sourceEn)))
            || (s_mainDict.contains(trimmed) && hasNonAsciiChar(s_mainDict.value(trimmed))))) {
        return;
    }

    s_extraDict.insert(sourceEn, targetJa);
    s_extraLowerDict.insert(sourceEn.toLower(), targetJa);

    QChar mnem;
    bool hadDouble = false;
    const QString clean = stripEnMnemonic(trimmed, &mnem, &hadDouble).trimmed();
    if (!clean.isEmpty() && !s_guardCaseInsensitive.contains(clean.toLower())
        && !s_canonicalMainKeys.contains(clean)) {
        QString cleanJa = targetJa;
        const int idx = cleanJa.indexOf(QLatin1String("(&"));
        if (idx >= 0 && idx + 3 < cleanJa.size() && cleanJa.at(idx + 3) == QLatin1Char(')'))
            cleanJa.remove(idx, 4);
        if (clean != sourceEn && !s_extraDict.contains(clean) && !s_mainDict.contains(clean))
            s_extraDict.insert(clean, cleanJa);
        if (!s_extraLowerDict.contains(clean.toLower()))
            s_extraLowerDict.insert(clean.toLower(), cleanJa);

        QString baseEn = clean;
        QString baseJa = cleanJa;
        if (baseEn.endsWith(QLatin1String("..."))) {
            baseEn.chop(3);
            baseEn = baseEn.trimmed();
            if (baseJa.endsWith(QLatin1String("...")))
                baseJa.chop(3);
            else if (baseJa.endsWith(QChar(0x2026)))
                baseJa.chop(1);
            baseJa = baseJa.trimmed();
        } else if (baseEn.endsWith(QLatin1Char(':'))) {
            baseEn.chop(1);
            baseEn = baseEn.trimmed();
            if (baseJa.endsWith(QChar(0xFF1A)) || baseJa.endsWith(QLatin1Char(':')))
                baseJa.chop(1);
            baseJa = baseJa.trimmed();
        }
        if (!baseEn.isEmpty() && baseEn != clean
            && !s_guardCaseInsensitive.contains(baseEn.toLower())
            && !s_extraLowerDict.contains(baseEn.toLower())) {
            s_extraLowerDict.insert(baseEn.toLower(), baseJa);
        }
    }
}

QString ToriGraphFullJapaneseTranslator::lookupJapanese(const QString &sourceEn)
{
    if (sourceEn.isEmpty())
        return QString();

    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();

    // Split leading and trailing whitespace (' ', '\t', '\r', '\n')
    int start = 0;
    const int len = sourceEn.size();
    while (start < len) {
        const QChar c = sourceEn.at(start);
        if (c == QLatin1Char(' ') || c == QLatin1Char('\t') || c == QLatin1Char('\r')
            || c == QLatin1Char('\n')) {
            ++start;
        } else {
            break;
        }
    }
    if (start == len)
        return QString();

    int end = len - 1;
    while (end >= start) {
        const QChar c = sourceEn.at(end);
        if (c == QLatin1Char(' ') || c == QLatin1Char('\t') || c == QLatin1Char('\r')
            || c == QLatin1Char('\n')) {
            --end;
        } else {
            break;
        }
    }

    const QString leadWs = (start > 0) ? sourceEn.left(start) : QString();
    const QString trailWs = (end < len - 1) ? sourceEn.mid(end + 1) : QString();
    const QString trimmed = sourceEn.mid(start, end - start + 1);

    // Never translate Arial or ToriGraph brand identifiers
    if (s_guardCaseInsensitive.contains(trimmed.toLower()))
        return QString();

    // 1. Exact match on full sourceEn (or trimmed) before math/single-letter guard
    QString exactFull = lookupDictExactLocked(sourceEn);
    if (!exactFull.isEmpty())
        return exactFull;

    QString exactTrimmed = lookupDictExactLocked(trimmed);
    if (!exactTrimmed.isEmpty())
        return leadWs + exactTrimmed + trailWs;

    if (isProtectedTokenLocked(trimmed))
        return QString();

    // 2. Exact case-sensitive match with trailing "..." / "…" / ":" / arrows stripped
    QString coreExact = lookupCoreLocked(trimmed, false);
    if (!coreExact.isEmpty())
        return leadWs + coreExact + trailWs;

    // 3. Exact case-sensitive match with &-mnemonic stripped (preserving mnemonic as (&X))
    QChar mnem;
    bool hadDoubleAmp = false;
    const QString clean = stripEnMnemonic(trimmed, &mnem, &hadDoubleAmp);
    const bool canUseClean = (clean != trimmed && !isProtectedTokenLocked(clean));
    if (canUseClean) {
        QString mnemExact = lookupCoreLocked(clean, false);
        if (!mnemExact.isEmpty()) {
            if (hadDoubleAmp)
                mnemExact = escapeStandaloneAmpersands(mnemExact);
            mnemExact = attachMnemonic(mnemExact, mnem);
            return leadWs + mnemExact + trailWs;
        }
    }

    // 4. Case-insensitive fallback on trimmed (and trailing punctuation/arrows stripped)
    QString coreRes = lookupCoreLocked(trimmed, true);
    if (!coreRes.isEmpty())
        return leadWs + coreRes + trailWs;

    // 5. Case-insensitive fallback with &-mnemonic stripped
    if (canUseClean) {
        QString mnemRes = lookupCoreLocked(clean, true);
        if (!mnemRes.isEmpty()) {
            if (hadDoubleAmp)
                mnemRes = escapeStandaloneAmpersands(mnemRes);
            mnemRes = attachMnemonic(mnemRes, mnem);
            return leadWs + mnemRes + trailWs;
        }
    }

    return QString();
}

void ToriGraphFullJapaneseTranslator::registerContextTranslation(const QString &context,
                                                                 const QString &sourceEn,
                                                                 const QString &targetJa)
{
    if (context.isEmpty() || sourceEn.isEmpty() || targetJa.isEmpty())
        return;
    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();
    s_contextDict.insert(contextKey(context, sourceEn), normalizeGlossaryImpl(sourceEn, targetJa));
}

QString ToriGraphFullJapaneseTranslator::lookupJapaneseInContext(const QString &context,
                                                                 const QString &sourceEn)
{
    if (context.isEmpty() || sourceEn.isEmpty())
        return QString();
    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();
    return lookupContextLocked(context, sourceEn);
}

QString ToriGraphFullJapaneseTranslator::normalizeGlossary(const QString &sourceEn,
                                                          const QString &ja)
{
    return normalizeGlossaryImpl(sourceEn, ja);
}

int ToriGraphFullJapaneseTranslator::builtinDictionarySize()
{
    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();
    return s_builtinEntryCount;
}

int ToriGraphFullJapaneseTranslator::contextDictionarySize()
{
    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();
    return s_contextDict.size();
}

int ToriGraphFullJapaneseTranslator::dictionarySize()
{
    QMutexLocker locker(&s_dictMutex);
    ensureInitializedLocked();
    return s_mainDict.size() + s_extraDict.size();
}
