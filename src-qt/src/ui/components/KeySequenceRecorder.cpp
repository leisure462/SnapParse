#include "KeySequenceRecorder.h"
#include <QKeySequence>

KeySequenceRecorder::KeySequenceRecorder(QWidget* parent) : QLineEdit(parent) {
    setReadOnly(true);
    setCursor(Qt::PointingHandCursor);
    setAlignment(Qt::AlignCenter);
    setPlaceholderText("点击录制快捷键");
    setFixedWidth(140);
}

void KeySequenceRecorder::setKeySequence(const QString& seq) {
    m_sequence = seq;
    setText(seq);
}

void KeySequenceRecorder::focusInEvent(QFocusEvent* event) {
    m_recording = true;
    setText("按下快捷键...");
    QLineEdit::focusInEvent(event);
}

void KeySequenceRecorder::focusOutEvent(QFocusEvent* event) {
    m_recording = false;
    setText(m_sequence.isEmpty() ? "未设置" : m_sequence);
    QLineEdit::focusOutEvent(event);
}

void KeySequenceRecorder::keyPressEvent(QKeyEvent* event) {
    if (!m_recording) {
        QLineEdit::keyPressEvent(event);
        return;
    }

    int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
        // Just modifiers, wait for regular key
        return;
    }

    if (key == Qt::Key_Escape || key == Qt::Key_Backspace) {
        m_sequence = "";
        setText("未设置");
        m_recording = false;
        clearFocus();
        emit sequenceChanged(m_sequence);
        return;
    }

    Qt::KeyboardModifiers mods = event->modifiers();
    QStringList parts;
    if (mods & Qt::ControlModifier) parts.append("Ctrl");
    if (mods & Qt::AltModifier) parts.append("Alt");
    if (mods & Qt::ShiftModifier) parts.append("Shift");
    if (mods & Qt::MetaModifier) parts.append("Win");

    QString keyStr = QKeySequence(key).toString();
    if (!keyStr.isEmpty()) {
        parts.append(keyStr);
    }

    if (!parts.isEmpty()) {
        m_sequence = parts.join("+");
        setText(m_sequence);
        m_recording = false;
        clearFocus();
        emit sequenceChanged(m_sequence);
    }
}
