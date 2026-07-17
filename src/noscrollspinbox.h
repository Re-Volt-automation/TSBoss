#pragma once
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QWheelEvent>

/// QDoubleSpinBox that accepts BOTH '.' and ',' as the decimal separator
/// while typing, regardless of locale. The "wrong" separator is rewritten
/// to the locale's own live in the editor, so on a decimal-comma system a
/// typed "49.3" becomes "49,3" instead of silently collapsing into 493.
class FlexibleDoubleSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    QValidator::State validate(QString &input, int &pos) const override
    {
        normalizeDecimalPoint(input);
        return QDoubleSpinBox::validate(input, pos);
    }
    double valueFromText(const QString &text) const override
    {
        QString t = text;
        normalizeDecimalPoint(t);
        return QDoubleSpinBox::valueFromText(t);
    }
private:
    // Same-length replacement, so the caller's cursor position stays valid.
    void normalizeDecimalPoint(QString &s) const
    {
        const QString dp = locale().decimalPoint();
        const QString other = (dp == QStringLiteral(",")) ? QStringLiteral(".")
                                                          : QStringLiteral(",");
        s.replace(other, dp);
    }
};

/// QDoubleSpinBox that ignores scroll-wheel events so that scrolling
/// the form does not accidentally change field values.
class NoScrollSpinBox : public FlexibleDoubleSpinBox
{
public:
    using FlexibleDoubleSpinBox::FlexibleDoubleSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override { e->ignore(); }
};

/// QSpinBox that ignores scroll-wheel events.
class NoScrollIntSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override { e->ignore(); }
};

/// QDoubleSpinBox that accepts wheel events only when it has keyboard focus.
class WheelWhenFocusedSpinBox : public FlexibleDoubleSpinBox
{
public:
    using FlexibleDoubleSpinBox::FlexibleDoubleSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override {
        if (hasFocus()) QDoubleSpinBox::wheelEvent(e);
        else            e->ignore();
    }
};

/// QSpinBox that accepts wheel events only when it has keyboard focus.
class WheelWhenFocusedIntSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override {
        if (hasFocus()) QSpinBox::wheelEvent(e);
        else            e->ignore();
    }
};
