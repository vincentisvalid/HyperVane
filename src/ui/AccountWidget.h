# Header file for Account Widgets
# src/ui/AccountWidget.h

#ifndef ACCOUNTWIDGET_H
#define ACCOUNTWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QLayout>

namespace HyperVane {

class AccountWidget : public QWidget{
    Q_OBJECT
public:
    explicit AccountWidget(QWidget *parent = nullptr);
    ~AccountWidget() override {}

private:
    QPushButton* loginButton;
    QPushButton* createButton;
    QLineEdit* usernameInput;
    QLineEdit* passwordInput;
    QLabel* statusLabel;

protected:
    void setupUi(); // Initializes widgets and layouts
    bool attemptLogin(const QString& user, const QString& pass);
};

} // namespace HyperVane

#endif // ACCOUNTWIDGET_H