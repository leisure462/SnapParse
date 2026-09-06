#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QStringList>

class SegmentedWidget : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedWidget(QWidget* parent = nullptr);

    void setOptions(const QList<QPair<QString, QString>>& options); // value, label
    QString currentValue() const { return m_currentValue; }
    void setCurrentValue(const QString& val);

signals:
    void valueChanged(const QString& val);

private slots:
    void handleButtonClicked(int id);

private:
    QHBoxLayout* m_layout = nullptr;
    QButtonGroup* m_buttonGroup = nullptr;
    QList<QPair<QString, QString>> m_options;
    QString m_currentValue;
};
