/*
    TikZiT - a GUI diagram editor for TikZ
    Copyright (C) 2018 Aleks Kissinger

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "tikzit.h"
#include "tikzassembler.h"
#include "tikzstyles.h"

#include <QFile>
#include <QFileDialog>
#include <QSettings>
#include <QDebug>
#include <QMessageBox>

// application-level instance of Tikzit
Tikzit *tikzit;

// font to use for node labels
QFont Tikzit::LABEL_FONT("Courrier", 9);

Tikzit::Tikzit() : _styleFile("[default]"), _activeWindow(0)
{
}

void Tikzit::init()
{
    QSettings settings("tikzit", "tikzit");

	// 19 standard xcolor colours
    _colNames <<
		"black" <<
		"gray" <<
		"darkgray" <<
		"lightgray" <<
		"white" <<

		"red" <<
		"orange" <<
		"yellow" <<
		"lime" <<
		"blue" <<
		"purple" <<

		"brown" <<
		"olive" <<
		"green" <<
		"teal" <<
		"cyan" <<

		"magenta" <<
		"violet" <<
		"pink";

	for (int i = 0; i < _colNames.length(); ++i) {
		_cols << QColor(_colNames[i]);
	}

    _mainMenu = new MainMenu();
    QMainWindow *dummy = new QMainWindow();

    _toolPalette = new ToolPalette(dummy);
    _propertyPalette = new PropertyPalette(dummy);
    //_stylePalette = new StylePalette(dummy);
    _styles = new TikzStyles(this);

    _styleEditor = new StyleEditor();

    //_stylePalette->show();
    _windows << new MainWindow();
    _windows[0]->show();

    QString styleFile = settings.value("previous-tikzstyles-file").toString();
    if (!styleFile.isEmpty()) loadStyles(styleFile);

    //connect(app, &QApplication::focusChanged, this, &focusChanged);
}

//QMenuBar *Tikzit::mainMenu() const
//{
//    return _mainMenu;
//}

QColor Tikzit::colorByIndex(int i)
{
    return _cols[i];
}

QColor Tikzit::colorByName(QString name)
{
    for (int i = 0; i < _colNames.length(); ++i) {
        if (_colNames[i] == name) return _cols[i];
    }

    QRegExp re(
      "rgb\\s*,\\s*255\\s*:\\s*"
      "red\\s*,\\s*([0-9]+)\\s*;\\s*"
      "green\\s*,\\s*([0-9]+)\\s*;\\s*"
      "blue\\s*,\\s*([0-9]+)\\s*"
    );

    if (re.exactMatch(name)) {
        QStringList cap = re.capturedTexts();
        //qDebug() << cap;
        return QColor(
                cap[1].toInt(),
                cap[2].toInt(),
                cap[3].toInt());
    }

    return QColor();
}

QString Tikzit::nameForColor(QColor col)
{
    for (int i = 0; i < _colNames.length(); ++i) {
        if (_cols[i] == col) return _colNames[i];
    }

    // if the color is not recognised, return it in tikz-readable RBG format
    return "rgb,255: red,"+ QString::number(col.red()) +
            "; green," + QString::number(col.green()) +
            "; blue," + QString::number(col.blue());
}

ToolPalette *Tikzit::toolPalette() const
{
    return _toolPalette;
}

PropertyPalette *Tikzit::propertyPalette() const
{
    return _propertyPalette;
}

void Tikzit::newDoc()
{
    MainWindow *w = new MainWindow();
    w->show();
    _windows << w;
}

MainWindow *Tikzit::activeWindow() const
{
    return _activeWindow;
}

void Tikzit::setActiveWindow(MainWindow *activeWindow)
{
    _activeWindow = activeWindow;
}

void Tikzit::removeWindow(MainWindow *w)
{
    _windows.removeAll(w);
    if (_activeWindow == w) {
        if (_windows.isEmpty()) {
            _activeWindow = 0;
            // TODO: check if we should quit when last window closed
            quit();
        } else _activeWindow = _windows[0];
    }
}

void Tikzit::open()
{
    QSettings settings("tikzit", "tikzit");
    QString fileName = QFileDialog::getOpenFileName(0,
                tr("Open File"),
                settings.value("previous-file-path").toString(),
                tr("TiKZ Files (*.tikz)"),
                nullptr,
                QFileDialog::DontUseNativeDialog);

	open(fileName);
}

void Tikzit::open(QString fileName)
{
	if (!fileName.isEmpty()) {
		if (_windows.size() == 1 &&
			_windows[0]->tikzDocument()->isClean() &&
			_windows[0]->tikzDocument()->shortName().isEmpty())
		{
			_windows[0]->open(fileName);
			_windows[0]->show();
		}
		else {
			MainWindow *w = new MainWindow();
			w->show();
			w->open(fileName);
			_windows << w;
		}
	}
}

void Tikzit::openTikzStyles() {
    QSettings settings("tikzit", "tikzit");
    QString fileName = QFileDialog::getOpenFileName(0,
                tr("Open File"),
                settings.value("previous-tikzstyles-path").toString(),
                tr("TiKZ Style Files (*.tikzstyles)"),
                nullptr,
                QFileDialog::DontUseNativeDialog);

    if (!fileName.isEmpty()) {
        QFileInfo fi(fileName);
        if (fi.exists() && loadStyles(fileName)) {
            QSettings settings("tikzit", "tikzit");
            settings.setValue("previous-tikzstyles-path", fi.absolutePath());
            settings.setValue("previous-tikzstyles-file", fileName);
        } else {
            // BAD STYLE FILE
        }
    }
}

bool Tikzit::loadStyles(QString fileName)
{
    QFileInfo fi(fileName);
    if (fi.exists()) {
        TikzStyles *st = new TikzStyles(this);
        if (st->loadStyles(fileName)) {
            _styleFile = fi.fileName();
            _styleFilePath = fi.absoluteFilePath();
            delete _styles;
            _styles = st;

            foreach (MainWindow *w, _windows) {
                w->tikzScene()->reloadStyles();
            }
            return true;
        } else {
            QMessageBox::warning(0,
                "Bad style file.",
                "Bad style file: '" + fileName + "'. Check the file is properly formatted and try to load it again.");
            return false;
        }

    } else {
        //settings.setValue("previous-tikzstyles-file", "");
        QMessageBox::warning(0, "Style file not found.", "Could not open style file: '" + fileName + "'.");
        return false;
    }
}

void Tikzit::showStyleEditor()
{
    _styleEditor->open();
}

QString Tikzit::styleFile() const
{
    return _styleFile;
}

QString Tikzit::styleFilePath() const
{
    return _styleFilePath;
}

//StylePalette *Tikzit::stylePalette() const
//{
//    return _stylePalette;
//}


TikzStyles *Tikzit::styles() const
{
    return _styles;
}

void Tikzit::quit()
{
    //_stylePalette->close();
    QApplication::quit();
}


