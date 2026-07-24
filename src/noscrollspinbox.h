#pragma once
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QLineEdit>
#include <QSpinBox>
#include <QWheelEvent>

// Shared focus behaviour: when a spinbox gains focus from a mouse click,
// select the whole entry so the first keystroke replaces it — otherwise a
// sentinel like "—" has to be erased by hand in every field. The click that
// grants focus places the caret *after* focusInEvent returns, so the
// select-all is deferred one event-loop turn. Later clicks (already
// focused) position the caret normally for digit edits.
template <typename SpinBase>
class SelectOnClickSpinBox : public SpinBase
{
public:
    using SpinBase::SpinBase;
protected:
    void focusInEvent(QFocusEvent *e) override
    {
        SpinBase::focusInEvent(e);
        if (e->reason() == Qt::MouseFocusReason) {
            QMetaObject::invokeMethod(this, [this] {
                if (auto *le = this->lineEdit()) le->selectAll();
            }, Qt::QueuedConnection);
        }
    }
};

/// QDoubleSpinBox that accepts BOTH '.' and ',' as the decimal separator
/// while typing, regardless of locale. The "wrong" separator is rewritten
/// to the locale's own live in the editor, so on a decimal-comma system a
/// typed "49.3" becomes "49,3" instead of silently collapsing into 493.
class FlexibleDoubleSpinBox : public SelectOnClickSpinBox<QDoubleSpinBox>
{
public:
    using SelectOnClickSpinBox<QDoubleSpinBox>::SelectOnClickSpinBox;
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
class NoScrollIntSpinBox : public SelectOnClickSpinBox<QSpinBox>
{
public:
    using SelectOnClickSpinBox<QSpinBox>::SelectOnClickSpinBox;
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
class WheelWhenFocusedIntSpinBox : public SelectOnClickSpinBox<QSpinBox>
{
public:
    using SelectOnClickSpinBox<QSpinBox>::SelectOnClickSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override {
        if (hasFocus()) QSpinBox::wheelEvent(e);
        else            e->ignore();
    }
};
