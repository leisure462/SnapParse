#pragma once

#include <QLineEdit>
#include <QKeyEvent>

class KeySequenceRecorder : public QLineEdit {
    Q_OBJECT
public:
    explicit KeySequenceRecorder(QWidget* parent = nullptr);

    QString keySequence() const { return m_sequence; }
    void setKeySequence(const QString& seq);

signals:
    void sequenceChanged(const QString& seq);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    bool m_recording = false;
    QString m_sequence;
};
