#if !defined (__DictEditDialog_H__)
#define __DictEditDialog_H__

#include <QDialog>
class QWidget;
class DictEditCanvas;
class QFrame;
class QGridLayout;
class QMenuBar;

class DictEditDialog : public QDialog
{
    Q_OBJECT

public:
    // constructors and destructors
    DictEditDialog(QWidget *parent);
    QWidget *getFrame();
    QMenuBar *getMenuBar();

public slots:
    void accept();
    void reject();

private:
    DictEditCanvas *canvas;
    QFrame *_frame;
    QGridLayout *_layout;
    QMenuBar *_menuBar;

};

#endif // if !defined (__DictEditDialog_H__)
