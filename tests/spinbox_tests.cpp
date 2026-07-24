#include "noscrollspinbox.h"
#include <QApplication>
#include <QLineEdit>
#include <cmath>
#include <cstdio>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// Expose the protected parse hooks for direct testing.
struct Probe : FlexibleDoubleSpinBox {
    using FlexibleDoubleSpinBox::validate;
    using FlexibleDoubleSpinBox::valueFromText;
    using FlexibleDoubleSpinBox::focusInEvent;
    using QAbstractSpinBox::lineEdit;
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    // ── Decimal-comma locale (nl_NL): '.' must work as decimal too ──
    {
        Probe s;
        s.setLocale(QLocale(QLocale::Dutch, QLocale::Netherlands));
        s.setDecimals(2);
        s.setRange(0.0, 2000.0);

        QString t = "49.3"; int pos = t.size();
        const auto st = s.validate(t, pos);
        check(st != QValidator::Invalid, "nl: '49.3' is not rejected");
        check(t == QString("49,3"),      "nl: '.' rewritten to ',' in the editor");

        check(std::fabs(s.valueFromText("49.3") - 49.3) < 1e-9,
              "nl: valueFromText parses '49.3'");
        check(std::fabs(s.valueFromText("49,3") - 49.3) < 1e-9,
              "nl: valueFromText still parses '49,3'");
    }

    // ── Decimal-point locale (C): ',' must work as decimal too ──────
    {
        Probe s;
        s.setLocale(QLocale::c());
        s.setDecimals(2);
        s.setRange(0.0, 2000.0);

        QString t = "49,3"; int pos = t.size();
        const auto st = s.validate(t, pos);
        check(st != QValidator::Invalid, "c: '49,3' is not rejected");
        check(t == QString("49.3"),      "c: ',' rewritten to '.' in the editor");

        check(std::fabs(s.valueFromText("49,3") - 49.3) < 1e-9,
              "c: valueFromText parses '49,3'");
    }

    // ── The classes the app actually instantiates inherit the fix ───
    {
        NoScrollSpinBox s;
        s.setLocale(QLocale(QLocale::Dutch, QLocale::Netherlands));
        s.setDecimals(2);
        s.setRange(0.0, 2000.0);
        s.findChild<QLineEdit *>()->setText("49.3");
        s.interpretText();
        check(std::fabs(s.value() - 49.3) < 1e-9,
              "NoScrollSpinBox: typed '49.3' commits as 49.3, not 493");
    }

    // ── Mouse focus selects the content, so typing overwrites the
    //    sentinel instead of appending around the em dash ─────────────
    //    (focusInEvent is driven directly: a headless test window can't
    //    become active, so real focus events are never delivered.)
    {
        Probe s;
        s.setDecimals(2);
        s.setRange(0.0, 100.0);
        s.setSpecialValueText("—");
        s.show();
        QFocusEvent ev(QEvent::FocusIn, Qt::MouseFocusReason);
        s.focusInEvent(&ev);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        check(s.lineEdit()->hasSelectedText(),
              "mouse focus selects the whole entry");
        check(s.lineEdit()->selectedText() == s.lineEdit()->text(),
              "selection covers the full text");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll spinbox tests passed.\n");
    return g_failures;
}
