#pragma once

#include <QDialog>
#include <QTextEdit>

class NoteDialog : public QDialog {
    Q_OBJECT
public:
    explicit NoteDialog(const QString& initialNote, QWidget* parent = nullptr);

    QString note() const { return m_textEdit->toPlainText(); }

private:
    QTextEdit* m_textEdit = nullptr;
};
