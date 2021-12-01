/*
 * Preferences.cpp - Adjustment of program settings
 *
 * Copyright (C) 2008-2021 Comer352L
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Preferences.h"



Preferences::Preferences(QMainWindow *parent, AbstractDiagInterface::interface_type *ifacetype, QString *ifacefilename, QString language) : QDialog(parent)
{
	_newinterfacetype = *ifacetype;
	_r_interfacetype = ifacetype;
	_newinterfacefilename = *ifacefilename;
	_r_interfacefilename = ifacefilename;
	_language_current = language;
	_language_old = language;
	_confirmed = false;
	// SET UP GUI:
	setupUi(this);
#ifndef SMALL_RESOLUTION
	// PLACE WINDOW:
	// get coordinates of FreeSSM_MainWindow
	QRect FreeSSM_MW_geometry;
	int FreeSSM_MW_Xpos, FreeSSM_MW_Ypos;
	FreeSSM_MW_geometry = parent->geometry();
	FreeSSM_MW_Xpos = FreeSSM_MW_geometry.x();
	FreeSSM_MW_Ypos = FreeSSM_MW_geometry.y();
	// calculate new window coordinates
	int x, y;
	x = (FreeSSM_MW_Xpos + 60);
	y = (FreeSSM_MW_Ypos + 40);
	// move window to desired coordinates
	move(x, y);
#else
	// https://bugreports.qt.io/browse/QTBUG-16034
	// Workaround for window not showing always fullscreen
	setWindowFlags(Qt::Window);
#endif

	// GUI-STYLES:
	_style_old = QApplication::style()->objectName();
	QStringList supStyles = QStyleFactory::keys();
	if (supStyles.size())
	{
#if QT_VERSION < 0x050000
		int styleindex = supStyles.indexOf( QRegExp( _style_old, Qt::CaseInsensitive, QRegExp::FixedString ) );
#else
		int styleindex = supStyles.indexOf( QRegularExpression( _style_old, QRegularExpression::CaseInsensitiveOption ) );
#endif
		// NOTE: default pattern syntax QRegExp::RegExp(2) doesn't work for "gtk+"/"GTK+" !
		guistyle_comboBox->insertItems(0, supStyles);
		guistyle_comboBox->setCurrentIndex( styleindex );
	}
	else
		guistyle_comboBox->setEnabled( false );
	// INTERFACES:
	int if_name_index = -1;
	if (_newinterfacetype == AbstractDiagInterface::interface_ATcommandControlled) // AT-comand controlled (e.g. ELM, AGV, Diamex)
	{
		interfaceType_comboBox->setCurrentIndex(2);
		selectInterfaceType(2);
		if_name_index = interfaceName_comboBox->findText(*_r_interfacefilename);
	}
	else	// Serial Pass-Through
	{
		interfaceType_comboBox->setCurrentIndex(0);
		selectInterfaceType(0);
		if_name_index = interfaceName_comboBox->findText(*_r_interfacefilename);
	}
	if (if_name_index >= 0)
	{
		interfaceName_comboBox->setCurrentIndex(if_name_index);
		selectInterfaceName(if_name_index);
	}
	else
	{
		if (interfaceName_comboBox->count() > 0)	// if min. 1 device available
		{
			interfaceName_comboBox->setCurrentIndex(0);
			selectInterfaceName(0);
			*_r_interfacefilename = _newinterfacefilename;
		}
		else
		{
			_newinterfacefilename = "";
			*_r_interfacefilename = "";
		}
	}
	// // CONNECT SIGNALS AND SLOTS:
	// connect( language_comboBox, SIGNAL( activated(int) ), this, SLOT( switchLanguage(int) ) );
	connect( guistyle_comboBox, SIGNAL( activated(QString) ), this, SLOT( switchGUIstyle(QString) ) );
	connect( interfaceType_comboBox, SIGNAL( activated(int) ), this, SLOT( selectInterfaceType(int) ) );
	connect( interfaceName_comboBox, SIGNAL( activated(int) ), this, SLOT( selectInterfaceName(int) ) );
	connect( testinterface_pushButton, SIGNAL( released() ), this, SLOT( interfacetest() ) );
	connect( ok_pushButton, SIGNAL( released() ), this, SLOT( ok() ) );
	connect( cancel_pushButton, SIGNAL( released() ), this, SLOT( close() ) );
	// NOTE: using released() instead of pressed() as workaround for a Qt-Bug occuring under MS Windows
}


Preferences::~Preferences()
{
// 	disconnect( language_comboBox, SIGNAL( activated(int) ), this, SLOT( switchLanguage(int) ) );
	disconnect( guistyle_comboBox, SIGNAL( activated(QString) ), this, SLOT( switchGUIstyle(QString) ) );
	disconnect( interfaceType_comboBox, SIGNAL( activated(int) ), this, SLOT( selectInterfaceType(int) ) );
	disconnect( interfaceName_comboBox, SIGNAL( activated(int) ), this, SLOT( selectInterfaceName(int) ) );
	disconnect( testinterface_pushButton, SIGNAL( released() ), this, SLOT( interfacetest() ) );
	disconnect( ok_pushButton, SIGNAL( released() ), this, SLOT( ok() ) );
	disconnect( cancel_pushButton, SIGNAL( released() ), this, SLOT( close() ) );
}

void Preferences::switchGUIstyle(QString style)
{
	QStyle *qstyle = NULL;
	if (style.size())
	{
		qstyle = QStyleFactory::create( style );
		if (qstyle)
			QApplication::setStyle( qstyle );
	}
}


void Preferences::selectInterfaceType(int index)
{
	interfaceName_comboBox->clear();
	_J2534libraryPaths.clear();
	_newinterfacefilename = "";
	QStringList deviceNames;
	// Serial Pass-Through or AT-comand controlled (e.g. ELM, AGV, Diamex)
	if (index == 1)
		_newinterfacetype = AbstractDiagInterface::interface_ATcommandControlled;
	else	// index == 0
		_newinterfacetype = AbstractDiagInterface::interface_serialPassThrough;
	interfaceName_label->setText(tr("Serial Port:"));
	std::vector<std::string> portlist = serialCOM::GetAvailablePorts();
	for (unsigned int k=0; k<portlist.size(); k++)
		deviceNames.push_back(QString::fromStdString(portlist.at(k)));
	if (deviceNames.size())
	{
		interfaceName_comboBox->addItems(deviceNames);
		interfaceName_comboBox->setCurrentIndex(0);
		selectInterfaceName(0);
	}
	interfaceName_comboBox->setEnabled(deviceNames.size());
	testinterface_pushButton->setEnabled(deviceNames.size());
	// NOTE: Never put anything else than a device-file into the interfaceName_comboBox !
}


void Preferences::selectInterfaceName(int index)
{
	if ((index < 0) || (index >= interfaceName_comboBox->count()))
		_newinterfacefilename = "";
	else{
			_newinterfacefilename = interfaceName_comboBox->currentText();
	}
}


void Preferences::interfacetest()
{
	QMessageBox *msgbox;
	FSSM_WaitMsgBox *waitmsgbox = NULL;
	QFont msgboxfont;
	// PREPARE INTERFACE:
	AbstractDiagInterface *diagInterface = NULL;
	if (_newinterfacetype == AbstractDiagInterface::interface_serialPassThrough)
	{
		diagInterface = new SerialPassThroughDiagInterface;
	}
	else if (_newinterfacetype == AbstractDiagInterface::interface_ATcommandControlled)
	{
		diagInterface = new ATcommandControlledDiagInterface;
	}
	else
	{
		displayErrorMsg(tr("The selected interface is not supported !"));
		displayErrorMsg(tr("Internal error:\nThe interface test for the selected interface is not yet implemented.\n=> Please report this as a bug."));
		return;
	}
	// OPEN INTERFACE:
	if (!diagInterface->open(_newinterfacefilename.toStdString()))
	{
		displayErrorMsg(tr("Couldn't open the diagnostic interface !\nPlease make sure that the device is not in use by another application."));
		delete diagInterface;
		return;
	}
	// DISPLAY INFO MESSAGE:
	int choice = QMessageBox::NoButton;
	msgbox = new QMessageBox( QMessageBox::Information, tr("Interface test"), tr("Please connect diagnostic interface to the vehicles\ndiagnostic connector and switch ignition on."), QMessageBox::NoButton, this);
	msgbox->addButton(tr("Start"), QMessageBox::AcceptRole);
	msgbox->addButton(tr("Cancel"), QMessageBox::RejectRole);
	msgboxfont = msgbox->font();
	msgboxfont.setPointSize(9);
	msgbox->setFont( msgboxfont );
	msgbox->show();
	choice = msgbox->exec();
	msgbox->close();
	delete msgbox;
	if (choice == QMessageBox::AcceptRole)
	{
		// START INTERFACE-TEST:
		bool icresult = false;
		bool retry = true;
		choice = 0;
		bool SSM1configOK = false;
		bool SSM2viaISO14230configOK = false;
		bool SSM2viaISO15765configOK = false;
		while (retry && !icresult)
		{
			char data = 0;
			// OUTPUT WAIT MESSAGE:
			waitmsgbox = new FSSM_WaitMsgBox(this, tr("Testing interface... Please wait !     "));
			waitmsgbox->show();
			// SSM2:
			SSMP2communication *SSMP2com = new SSMP2communication(diagInterface);
			SSM2viaISO14230configOK = diagInterface->connect(AbstractDiagInterface::protocol_SSM2_ISO14230);
			if (SSM2viaISO14230configOK)
			{
				SSMP2com->setCUaddress(0x10);
				unsigned int addr = 0x61;
				icresult = SSMP2com->readMultipleDatabytes('\x0', &addr, 1, &data);
				if (!icresult)
				{
					SSMP2com->setCUaddress(0x01);
					icresult = SSMP2com->readMultipleDatabytes('\x0', &addr, 1, &data);
					if (!icresult)
					{
						SSMP2com->setCUaddress(0x02);
						icresult = SSMP2com->readMultipleDatabytes('\x0', &addr, 1, &data);
					}
				}
				diagInterface->disconnect();
			}
			if (!icresult)
			{
				SSM2viaISO15765configOK = diagInterface->connect(AbstractDiagInterface::protocol_SSM2_ISO15765);
				if (SSM2viaISO15765configOK)
				{
					SSMP2com->setCUaddress(0x7E0);
					unsigned int addr = 0x61;
					icresult = SSMP2com->readMultipleDatabytes('\x0', &addr, 1, &data);
					diagInterface->disconnect();
				}
			}
			delete SSMP2com;
			// SSM1:
			if (!icresult)
			{
				SSM1configOK = diagInterface->connect(AbstractDiagInterface::protocol_SSM1);
				if (SSM1configOK)
				{
					SSMP1communication *SSMP1com = new SSMP1communication(diagInterface, SSM1_CU_Engine);
					icresult = SSMP1com->readAddress(0x00, &data);
					if (!icresult)
					{
						SSMP1com->selectCU(SSM1_CU_Transmission);
						icresult = SSMP1com->readAddress(0x00, &data);
						delete SSMP1com;
					}
					diagInterface->disconnect();
				}
			}
			// CLOSE WAIT MESSAGE:
			waitmsgbox->close();
			delete waitmsgbox;
			// DISPLAY TEST RESULT:
			QString resultText;
			if (icresult)
				resultText = tr("Interface test successful !");
			else
				resultText = tr("Interface test failed !");
			if (!SSM1configOK && !SSM2viaISO14230configOK && !SSM2viaISO15765configOK)	// => test must have failed
			{
				if (_newinterfacetype == AbstractDiagInterface::interface_serialPassThrough)
					resultText += "\n\n" + tr("The selected serial port can not be configured for the SSM1- and SSM2-protocol.");
				else
					resultText += "\n\n" + tr("The selected interface does not support the SSM1- and SSM2-protocol.");
			}
			else if (!icresult)
			{
				resultText += "\n\n" + tr("Please make sure that the interface is connected properly and ignition is switched ON.");
			}
			if (!SSM1configOK || !SSM2viaISO14230configOK || !SSM2viaISO15765configOK)
			{
				resultText += "\n\n" + tr("WARNING:");
				if (!SSM1configOK)
				{
					if (_newinterfacetype == AbstractDiagInterface::interface_serialPassThrough)
						resultText += '\n' + tr("The selected serial port can not be configured for the SSM1-protocol.");
					else
						resultText += '\n' + tr("The selected interface does not support the SSM1-protocol.");
				}
				if (!SSM2viaISO14230configOK)
				{
					if (_newinterfacetype == AbstractDiagInterface::interface_serialPassThrough)
						resultText += '\n' + tr("The selected serial port can not be configured for the SSM2-protocol via ISO-14230.");
					else
						resultText += '\n' + tr("The selected interface does not support the SSM2-protocol via ISO-14230.");
				}
				if (!SSM2viaISO15765configOK)
				{
					if (_newinterfacetype == AbstractDiagInterface::interface_serialPassThrough)
						resultText += '\n' + tr("Serial Pass-Through interfaces do not support the SSM2-protocol via ISO-15765.");
					else
						resultText += '\n' + tr("The selected interface does not support the SSM2-protocol via ISO-15765.");
				}
			}
			if (icresult)
				msgbox = new QMessageBox(QMessageBox::Information, tr("Interface test"), resultText, QMessageBox::Ok, this);
			else
			{
				msgbox = new QMessageBox(QMessageBox::Critical, tr("Interface test"), resultText, QMessageBox::NoButton, this);
				msgbox->addButton(tr("Retry"), QMessageBox::AcceptRole);
				msgbox->addButton(tr("Cancel"), QMessageBox::RejectRole);
			}
			msgboxfont = msgbox->font();
			msgboxfont.setPointSize(9);
			msgbox->setFont( msgboxfont );
			msgbox->show();
			choice = msgbox->exec();
			msgbox->close();
			delete msgbox;
			if (!icresult && (choice != QMessageBox::AcceptRole))
				retry = false;
		}
	}
	// CLOSE INTERFACE:
	if (!diagInterface->close())
		displayErrorMsg(tr("Couldn't close the diagnostic interface !"));
	delete diagInterface;
}


void Preferences::ok()
{
	// RETURN CURRENT INTERFACE-TYPE AND -NAME:
	*_r_interfacetype = _newinterfacetype;
	*_r_interfacefilename = _newinterfacefilename;
	// SAVE PREFERENCES TO FILE:
	QFile prefsfile(QDir::homePath() + "/FreeSSM.prefs");
	if (prefsfile.open(QIODevice::WriteOnly | QIODevice::Text))	// try to open/built preferences file
	{
		QString stylename = QApplication::style()->objectName();
		// rewrite file completely:
		prefsfile.write(_newinterfacefilename.toUtf8() + "\n");	// save interface name
		prefsfile.write(_language_current.toUtf8() + "\n");	// save language
		prefsfile.write(stylename.toUtf8() + "\n");		// save preferred GUI-style
		prefsfile.write(QString::number(_newinterfacetype).toUtf8() + "\n"); // save interface type
		prefsfile.close();
	}
	else
	{
		QMessageBox msg( QMessageBox::Warning, tr("Error"), tr("Couldn't save preferences to file !\nTo prevent this failure in the future, ensure write access\nto your home directory and file ''FreeSSM.prefs''."), QMessageBox::Ok, this);
		QFont msgfont = msg.font();
		msgfont.setPointSize(9);
		msg.setFont( msgfont );
		msg.show();
		msg.exec();
		msg.close();
	}
	_confirmed = true;	// IMPORTANT: prevents undo of all changes in close event handler
	close();		// close window (delete is called automaticly)
}


void Preferences::closeEvent(QCloseEvent *event)
{
	if (!_confirmed)
	{
		// Switch back to old translation:
		QLocale loc( _language_old );
		// Switch back to old GUI-style:
		switchGUIstyle( _style_old );
	}
	event->accept();
}


void Preferences::displayErrorMsg(QString errormsg)
{
	QMessageBox *msgbox;
	QFont msgboxfont;
	msgbox = new QMessageBox( QMessageBox::Critical, tr("Error"), errormsg, QMessageBox::Ok, this);
	msgboxfont = msgbox->font();
	msgboxfont.setPointSize(9);
	msgbox->setFont( msgboxfont );
	msgbox->show();
	msgbox->exec();
	msgbox->close();
	delete msgbox;
}

