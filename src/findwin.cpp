#include "findwin.h"
#include "scintillaeditview.h"
#include "ccnotepad.h"
#include "progresswin.h"
#include "doctypelistview.h"
#include "filemanager.h"

#include <QMimeDatabase>
#include <QRadioButton>
#include <QMessageBox>
#include <QFileDialog>
#include <functional>
#include <QDebug>

FindWin::FindWin(QWidget *parent):QMainWindow(parent), m_editTabWidget(nullptr), m_isFindFirst(true), m_findHistory(nullptr), \
	pEditTemp(nullptr), m_curEditWin(nullptr), m_isStatic(false)
{
	ui.setupUi(this);

	m_BackwardDir = false;
	m_matchWhole = false;
	m_matchCase = false;
	m_matchWrap = true;
	m_extend = false;
	m_searchMode = 1;

	connect(ui.findModeRegularBt, &QRadioButton::toggled, this, &FindWin::slot_findModeRegularBtChange);
	connect(ui.replaceModeRegularBt, &QRadioButton::toggled, this, &FindWin::slot_replaceModeRegularBtChange);
	connect(ui.dealFileType, &QCheckBox::stateChanged, this,&FindWin::slot_dealFileTypeChange);
	connect(ui.clearBt, &QAbstractButton::clicked, this, &FindWin::sign_clearResult);
	connect(ui.findClearBt, &QAbstractButton::clicked, this, &FindWin::sign_clearResult);

#if 0 //这样是无效的，记住一下，不删除，避免后面再做无用功
	Qt::WindowFlags m_flags = windowFlags();
	//这样使得窗口在上面，不至于总是到后面被挡住。
#if defined (Q_MAC_OS)
    setWindowFlags(m_flags | Qt::WindowStaysOnTopHint);
#endif
#endif
	raise();
	setFocusPolicy(Qt::StrongFocus);
	setFocus();

	ui.findinfilesTab->setAttribute(Qt::WA_StyledBackground);

	ui.findComboBox->installEventFilter(this);
}

FindWin::~FindWin()
{
	m_findHistory = nullptr;
	if (pEditTemp != nullptr)
	{
		delete pEditTemp;
		pEditTemp = nullptr;
}
}

void FindWin::slot_dealFileTypeChange(int state)
{
	if (state == Qt::Checked)
	{
		ui.fileType->setEnabled(true);
	}
	else
	{
		ui.fileType->setEnabled(false);
	}
}

//自动调整当前窗口的状态，如果发生了变化，则需要认定为是首次查找
QWidget* FindWin::autoAdjustCurrentEditWin()
{
	QWidget* pw = m_editTabWidget->currentWidget();

	if (m_curEditWin != pw)
	{
		m_curEditWin = pw;
		m_isFindFirst = true;
	}
	return pw;
}


void FindWin::setCurrentTab(FindTabIndex index)
{
	ui.findinfilesTab->setCurrentIndex(index);
	if (FIND_TAB == index)
	{
		ui.findComboBox->setFocus();
	}
	else if(REPLACE_TAB == index)
	{
		ui.replaceTextBox->setFocus();
	}

    raise();
}

void FindWin::setTabWidget(QTabWidget *editTabWidget)
{
	m_editTabWidget = editTabWidget;
}

void FindWin::setFindText(QString &text)
{
	ui.findComboBox->setEditText(text);
	ui.replaceTextBox->setEditText(text);
	ui.markTextBox->setEditText(text);
	ui.dirFindWhat->setEditText(text);

	addFindHistory(text);
}

void FindWin::disableReplace()
{
	ui.tab_replace->setEnabled(false);
}

void FindWin::setFindHistory(QList<QString>* findHistory)
{
	m_findHistory = findHistory;

	if ((m_findHistory != nullptr) && !m_findHistory->isEmpty())
	{
		ui.findComboBox->addItems(*m_findHistory);
		ui.replaceTextBox->addItems(*m_findHistory);
		ui.dirFindWhat->addItems(*m_findHistory);
		ui.findComboBox->clearEditText();
		ui.replaceTextBox->clearEditText();
	}
}

//标记高亮所有word单词
void FindWin::markAllWord(QString & word)
{
	ui.markTextBox->setCurrentText(word);
	ui.findinfilesTab->setCurrentIndex(3);
	//发现中文在全词匹配下面不能匹配到单词。最好是英文全词匹配，中文不需要
	//但是好像没有一个现成的方法来判断word中的字符。暂时不做全词匹配
	ui.markMatchWholeBox->setChecked(false);
	ui.markMatchCaseBox->setChecked(true);
	slot_markAll();
}

//删除行首尾的空白字符
void FindWin::removeLineHeadEndBlank(int mode)
{
	QWidget* pw = autoAdjustCurrentEditWin();
	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		if (pEdit->isReadOnly())
		{
			ui.statusbar->showMessage(tr("The ReadOnly document does not allow this operation."), 8000);
			QApplication::beep();
			return;
		}
		ui.findinfilesTab->setCurrentIndex(1);

		if (mode == 1)
		{
			ui.replaceTextBox->setCurrentText("^\\s+");
		}
		else if (mode == 2)
		{
			ui.replaceTextBox->setCurrentText("\\s+$");
		}
		ui.replaceWithBox->setCurrentText("");
		
		ui.replaceModeRegularBt->setChecked(true);

		m_isStatic = true;

		slot_replaceAll();

		m_isStatic = false;
	}
}

void FindWin::focusInEvent(QFocusEvent * ev)
{
	QWidget::focusInEvent(ev);
	setWindowOpacity(1.0);
}

void FindWin::focusOutEvent(QFocusEvent * ev)
{
	QWidget::focusOutEvent(ev);

	if (this->isActiveWindow())
	{
		//当前还是活动窗口，不设置透明
		return;
	}
	else
	{
		setWindowOpacity(0.6);
        //static int i = 0;
        //qDebug() << ++i;
	}

}


bool FindWin::eventFilter(QObject* watched, QEvent *event)
{
	if (watched == ui.findComboBox)
	{
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent *ke = static_cast<QKeyEvent*>(event);
			if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return)
			{
				emit ui.findTextNext->click();
				return true;        //该事件已经被处理
	}
			return false;
		}
		else
	{
			return false;   
	}
	}

	return QWidget::eventFilter(watched, event);     // 最后将事件交给上层对话框
}


//从ui读取参数配置到成员变量
void FindWin::updateParameterFromUI()
{
	bool extend = false;

	if (ui.findinfilesTab->currentIndex() == 0)
	{
		int searchMode = 0;

		if (ui.findModeNormalBt->isChecked())
		{
			searchMode = 1;
			extend = false;
		}
		else if (ui.findModeRegularBt->isChecked())
		{
			searchMode = 2;
			extend = false;
		}
		else if (ui.findModeExtendBt->isChecked())
		{
			searchMode = 1;
			extend = true;
		}

		if (m_searchMode != searchMode)
		{
			m_searchMode = searchMode;
			m_isFindFirst = true;
		}

		if (m_expr != ui.findComboBox->currentText())
		{
			m_expr = ui.findComboBox->currentText();
			m_isFindFirst = true;
		}

		if (m_BackwardDir != ui.findBackwardBox->isChecked())
		{
			m_BackwardDir = ui.findBackwardBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchWhole != ui.findMatchWholeBox->isChecked())
		{
			m_matchWhole = ui.findMatchWholeBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchCase != ui.findMatchCaseBox->isChecked())
		{
			m_matchCase = ui.findMatchCaseBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchWrap != ui.findWrapBox->isChecked())
		{
			m_matchWrap = ui.findWrapBox->isChecked();
			m_isFindFirst = true;
		}
	}
	else if (ui.findinfilesTab->currentIndex() == 1)
	{
		int searchMode = 0;

		if (ui.replaceModeNormalBox->isChecked())
		{
			searchMode = 1;
			extend = false;
		}
		else if (ui.replaceModeRegularBt->isChecked())
		{
			searchMode = 2;
			extend = false;
		}
		else if (ui.replaceModeExtendBox->isChecked())
		{
			searchMode = 1;
			extend = true;
		}

		if (m_searchMode != searchMode)
		{
			m_searchMode = searchMode;
			m_isFindFirst = true;
		}

		if (m_expr != ui.replaceTextBox->currentText())
		{
			m_expr = ui.replaceTextBox->currentText();
			m_isFindFirst = true;
		}

		if (m_replaceWithText != ui.replaceWithBox->currentText())
		{
			m_replaceWithText = ui.replaceWithBox->currentText();
			m_isFindFirst = true;
		}

		if (m_BackwardDir != ui.replaceBackwardBox->isChecked())
		{
			m_BackwardDir = ui.replaceBackwardBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchWhole != ui.replaceMatchWholeBox->isChecked())
		{
			m_matchWhole = ui.replaceMatchWholeBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchCase != ui.replaceMatchCaseBox->isChecked())
		{
			m_matchCase = ui.replaceMatchCaseBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchWrap != ui.replaceWrapBox->isChecked())
		{
			m_matchWrap = ui.replaceWrapBox->isChecked();
			m_isFindFirst = true;
		}
	}
	else if (ui.findinfilesTab->currentIndex() == 2)
	{
		int searchMode = 0;

		if (ui.dirFindModeNormalBox->isChecked())
		{
			searchMode = 1;
			extend = false;
		}
		else if (ui.dirFindModeRegularBt->isChecked())
		{
			searchMode = 2;
			extend = false;
		}
		else if (ui.dirFindModeExtendBox->isChecked())
		{
			searchMode = 1;
			extend = true;
		}

		if (m_searchMode != searchMode)
		{
			m_searchMode = searchMode;
			m_isFindFirst = true;
		}

		if (m_expr != ui.dirFindWhat->currentText())
		{
			m_expr = ui.dirFindWhat->currentText();
			m_isFindFirst = true;
		}

		if (m_replaceWithText != ui.dirReplaceWhat->currentText())
		{
			m_replaceWithText = ui.dirReplaceWhat->currentText();
			m_isFindFirst = true;
		}

		m_BackwardDir = false;

		if (m_matchWhole != ui.dirFindMatchWholeBox->isChecked())
		{
			m_matchWhole = ui.dirFindMatchWholeBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchCase != ui.dirFindMatchCaseBox->isChecked())
		{
			m_matchCase = ui.dirFindMatchCaseBox->isChecked();
			m_isFindFirst = true;
		}

		m_matchWrap = true;
	}
	else if (ui.findinfilesTab->currentIndex() == 3)
	{
		int searchMode = 0;

		if (ui.markModeNormalBox->isChecked())
		{
			searchMode = 1;
			extend = false;
		}
		else if (ui.markModeRegularBt->isChecked())
		{
			searchMode = 2;
			extend = false;
		}
		else if (ui.markModeExtendBox->isChecked())
		{
			searchMode = 1;
			extend = true;
		}

		if (m_searchMode != searchMode)
		{
			m_searchMode = searchMode;
			m_isFindFirst = true;
		}

		if (m_expr != ui.markTextBox->currentText())
		{
			m_expr = ui.markTextBox->currentText();
			m_isFindFirst = true;
		}

		m_BackwardDir = false;

		if (m_matchWhole != ui.markMatchWholeBox->isChecked())
		{
			m_matchWhole = ui.markMatchWholeBox->isChecked();
			m_isFindFirst = true;
		}

		if (m_matchCase != ui.markMatchCaseBox->isChecked())
		{
			m_matchCase = ui.markMatchCaseBox->isChecked();
			m_isFindFirst = true;
		}

		m_matchWrap = true;
	}

	m_re = ((m_searchMode == 2) ? true : false);

	if (m_cs != m_matchCase)
	{
		m_cs = m_matchCase;
	}

	if (m_wo != m_matchWhole)
	{
		m_wo = m_matchWhole;
	}

	if (m_wrap != m_matchWrap)
	{
		m_wrap = m_matchWrap;
	}

	if (m_extend != extend)
	{
		m_extend = extend;
		m_isFindFirst = true;
	}

	m_forward = !m_BackwardDir;
}

void FindWin::addFindHistory(QString &text)
{
	if ((m_findHistory != nullptr) && (-1 == m_findHistory->indexOf(text)))
	{
		m_findHistory->push_front(text);

		if (m_findHistory->size() >= 15)
		{
			m_findHistory->takeLast();
		}

		ui.findComboBox->insertItem(0, text);
		ui.replaceTextBox->insertItem(0, text);
		ui.dirFindWhat->insertItem(0,text);
		ui.markTextBox->insertItem(0, text);
	}
}

//检查是否是第一次查找，凡是参数变化了，则认定为是第一次查找。
//因为查找分firstFirst和firstNext，则是qscint特性决定的。所以正确识别第一次查找是必要的
bool FindWin::isFirstFind()
{
	return m_isFindFirst;
}

//const QChar *constData() const

bool readBase(const QChar * str, int * value, int base, int size)
{
	int i = 0, temp = 0;
	*value = 0;
	QChar max = QChar(QChar('0').digitValue() + base - 1);
	QChar current;
	while (i < size)
	{
		current = str[i];
		if (current >= 'A')
		{
			current = current.digitValue() & 0xdf;
			current = current.digitValue() - ('A' - '0' - 10);
		}
		else if (current > '9')
			return false;

		if (current >= '0' && current <= max)
		{
			temp *= base;
			temp += (current.digitValue() - '0');
		}
		else
		{
			return false;
		}
		++i;
	}
	*value = temp;
	return true;
}


int convertExtendedToString(QString& query, QString &result)
{	//query may equal to result, since it always gets smaller
	int i = 0, j = 0;

	int length = query.length();
	int charLeft = length;
	QChar current;
	while (i < length)
	{	//because the backslash escape quences always reduce the size of the generic_string, no overflow checks have to be made for target, assuming parameters are correct
		current = query.at(i);
		--charLeft;
		if (current == '\\' && charLeft)
		{	//possible escape sequence
			++i;
			--charLeft;
			current = query.at(i);
			switch (current.toLatin1())
			{
			case 'r':
				result[j] = '\r';
				break;
			case 'n':
				result[j] = '\n';
				break;
			case '0':
				result[j] = '\0';
				break;
			case 't':
				result[j] = '\t';
				break;
			case '\\':
				result[j] = '\\';
				break;
			case 'b':
			case 'd':
			case 'o':
			case 'x':
			case 'u':
			{
				int size = 0, base = 0;
				if (current == 'b')
				{	//11111111
					size = 8, base = 2;
				}
				else if (current == 'o')
				{	//377
					size = 3, base = 8;
				}
				else if (current == 'd')
				{	//255
					size = 3, base = 10;
				}
				else if (current == 'x')
				{	//0xFF
					size = 2, base = 16;
				}
				else if (current == 'u')
				{	//0xCDCD
					size = 4, base = 16;
				}

				if (charLeft >= size)
				{
					int res = 0;
					if (readBase(query.constData() + (i + 1), &res, base, size))
					{
						result[j] = QChar(res);
						i += size;
						break;
					}
				}
				//not enough chars to make parameter, use default method as fallback
			}

			default:
			{	//unknown sequence, treat as regular text
				result[j] = '\\';
				++j;
				result[j] = current;
				break;
			}
			}
		}
		else
		{
			result[j] = query[i];
		}
		++i;
		++j;
	}
	return j;
}

void FindWin::showCallTip(QsciScintilla* pEdit, int pos)
{
	QString zeroString(tr("find-regex-zero-length-match"));
	QByteArray bytes = zeroString.toUtf8();
	bytes.append('\0');

	/*int size = bytes.size();
	char* newStr = new char[size + 1];
	memcpy(newStr, bytes.data(), size);
	newStr[bytes.size()] = '\0';*/

	pEdit->SendScintilla(SCI_CALLTIPSHOW, pos, (void*)(bytes.data()));
	/*delete[]newStr;*/
}

//删除空白行
void FindWin::removeEmptyLine(bool isBlankContained)
{
	QWidget* pw = autoAdjustCurrentEditWin();
	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		if (pEdit->isReadOnly())
		{
			ui.statusbar->showMessage(tr("The ReadOnly document does not allow this operation."), 8000);
			QApplication::beep();
			return;
		}
		ui.findinfilesTab->setCurrentIndex(1);

		if (isBlankContained)
		{
			ui.replaceTextBox->setCurrentText("^[\\t ]*$(\\r\\n|\\r|\\n)");
		}
		else 
		{
			ui.replaceTextBox->setCurrentText("^$(\\r\\n|\\r|\\n)");
		}
		ui.replaceWithBox->setCurrentText("");

		ui.replaceModeRegularBt->setChecked(true);

		m_isStatic = true;

		slot_replaceAll();

		m_isStatic = false;
	}
}

/*处理查找时零长的问题。一定要处理，否则会死循环，因为每次都在原地查找。
* 就是把下次查找的startpos往前一个，否则每次都从这个startpos找到自己
*/
void FindWin::dealWithZeroFound(QsciScintilla* pEdit)
{
	FindState& state = pEdit->getLastFindState();

	if (state.targstart == state.targend)
	{
		//强行加1，否则每次查找总是在同一个地方。
		state.startpos++;
	}
}

/*处理查找时零长的问题。一定要处理，否则会死循环，因为每次都在原地查找。
* 就是把下次查找的startpos往前一个，否则每次都从这个startpos找到自己
* 和dealWithZeroFound是一样的，就是要显示消息而已
*/
void FindWin::dealWithZeroFoundShowTip(QsciScintilla* pEdit, bool isShowTip)
{
	if (!isShowTip)
	{
		return dealWithZeroFound(pEdit);
	}

	FindState& state = pEdit->getLastFindState();

	ui.statusbar->showMessage(tr("target info linenum %1 pos is %2 - %3").arg(state.linenum + 1).arg(state.targstart).arg(state.targend), 8000);

	if (state.targstart == state.targend)
	{
		//强行加1，否则每次查找总是在同一个地方。
		//这里有个问题，如果是反向查找，则应该是pos减去1才对。
		//不过只有在正则查找才会出现零长，而正则查找时我们不允许反向查找。反向是回调的。
		state.startpos++;
		showCallTip(pEdit, state.targstart);
	}
}


//一旦修改条件发生变化，则认定为第一次查找
void FindWin::slot_findNext()
{
	if (ui.findComboBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}

	updateParameterFromUI();

	QWidget* pw = autoAdjustCurrentEditWin();


	QsciScintilla* pEdit = dynamic_cast<QsciScintilla*>(pw);

	//第一次查找
	if (isFirstFind())
	{
		if (pEdit != nullptr)
		{
			QString whatFind = ui.findComboBox->currentText();

			addFindHistory(whatFind);

			if (m_extend)
			{
				QString extendFind;
				convertExtendedToString(whatFind, extendFind);
				whatFind = extendFind;
			}

			if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, m_wrap, m_forward, FINDNEXTTYPE_FINDNEXT, -1,-1,true,false,false))
			{
				ui.statusbar->showMessage(tr("cant't find text \'%1\'").arg(m_expr),8000);
				QApplication::beep();
				m_isFindFirst = true;
			}
			else
			{
				m_isFindFirst = false;
				dealWithZeroFoundShowTip(pEdit);
			}
		}
	}
	else
	{
		//查找下一个
		if (pEdit != nullptr)
		{
			if (!pEdit->findNext())
			{
				ui.statusbar->showMessage(tr("no more find text \'%1\'").arg(m_expr),8000);
				m_isFindFirst = true;
				QApplication::beep();
			}
			else
			{
				dealWithZeroFoundShowTip(pEdit);
			}
		}
	}
}

//查找计数
void FindWin::slot_findCount()
{
	if (ui.findComboBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
}

	QWidget* pw = autoAdjustCurrentEditWin();
	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		if (pEdit->isReadOnly())
		{
			ui.statusbar->showMessage(tr("The ReadOnly document does not allow this operation."), 8000);
			QApplication::beep();
			return;
		}

		updateParameterFromUI();

		int srcPostion = pEdit->execute(SCI_GETCURRENTPOS);
		int firstDisLineNum = pEdit->execute(SCI_GETFIRSTVISIBLELINE);

		int countNums = 0;
		//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
		QString whatFind = ui.findComboBox->currentText();

		//这里不能直接修改results.findText的值，该值在外部显示还需要。如果修改则会显示紊乱
		if (m_extend)
		{
			QString extendFind;
			convertExtendedToString(whatFind, extendFind);
			whatFind = extendFind;
		}

		//这里的forward一定要是true。回环一定是false
		if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, false, true, FINDNEXTTYPE_FINDNEXT, 0, 0))
		{
			ui.statusbar->showMessage(tr("count %1 times with \'%2\'").arg(countNums).arg(m_expr), 8000);
			QApplication::beep();

			m_isFindFirst = true;

			return;
		}
		else
		{
			dealWithZeroFound(pEdit);
		}

		++countNums;

		//找到了,增加计数
		while (pEdit->findNext())
		{
			++countNums;

			dealWithZeroFound(pEdit);
		}

		pEdit->execute(SCI_GOTOPOS, srcPostion);
		pEdit->execute(SCI_SETFIRSTVISIBLELINE, firstDisLineNum);
		pEdit->execute(SCI_SETXOFFSET, 0);

		//全部替换后，下次查找，必须算第一次查找
		m_isFindFirst = true;
		ui.statusbar->showMessage(tr("count %1 times with \'%2\'").arg(countNums).arg(m_expr), 8000);
	}
	else
	{
		ui.statusbar->showMessage(tr("The mode of the current document does not allow this operation."), 8000);
		QApplication::beep();
	}
}

//去掉行尾的\n\r符号
static QString trimmedEnd(QString lineText)
{
	if (lineText.endsWith("\r\n"))
	{
		return lineText.mid(0, lineText.length()-2);
	}
	if (lineText.endsWith("\r"))
	{
		return lineText.mid(0, lineText.length()-1);
	}
	if (lineText.endsWith("\n"))
	{
		return lineText.mid(0, lineText.length()-1);
	}
	return lineText;
}

void FindWin::addCurFindRecord(ScintillaEditView* pEdit, FindRecords& recordRet,bool isMark)
{
	FindRecord aRecord;

	//看了源码，当前查找到的结果，是会被选中的。所以可通过选中范围，来记录当前被查找中的结果
	//光标在选择词的尾部下一个位置
#if 0
	aRecord.pos = pEdit->execute(SCI_GETSELECTIONSTART);
	aRecord.end = pEdit->execute(SCI_GETSELECTIONEND);
#endif
	//加速
	FindState& state = pEdit->getLastFindState();
	aRecord.pos = state.targstart;
	aRecord.end = state.targend;

	//mark模式不需要这么多信息，可直接返回
	if (!isMark)
	{
	aRecord.lineNum = pEdit->execute(SCI_LINEFROMPOSITION, aRecord.pos);
	aRecord.lineStartPos = pEdit->execute(SCI_POSITIONFROMLINE, aRecord.lineNum);
	int lineLens = pEdit->execute(SCI_LINELENGTH, aRecord.lineNum);

	if (lineLens <= 0)
	{
		return;
	}

	char* lineText = new char[lineLens + 1];
	memset(lineText, 0, lineLens + 1);

	//这里有个bug,是qscint的，查找最后一行，会漏掉最后一个字符
	pEdit->execute(SCI_GETLINE, aRecord.lineNum, reinterpret_cast<sptr_t>(lineText));

	//务必要去掉行位的换行，否则显示结果列表会显示换行
	aRecord.lineContents = trimmedEnd(QString(lineText));

	delete[]lineText;
	}

	recordRet.records.append(aRecord);
}

void FindWin::slot_findAllInCurDoc()
{
	if (ui.findComboBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}

	QWidget* pw = autoAdjustCurrentEditWin();
	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		if (pEdit->isReadOnly())
		{
			ui.statusbar->showMessage(tr("The ReadOnly document does not allow this operation."), 8000);
			QApplication::beep();
			return;
		}

		FindRecords results;
		results.pEdit = pEdit;

		results.findFilePath = pw->property("filePath").toString();

		updateParameterFromUI();

		int srcPostion = pEdit->execute(SCI_GETCURRENTPOS);
		int firstDisLineNum = pEdit->execute(SCI_GETFIRSTVISIBLELINE);

		int replaceNums = 0;
		//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
		QString whatFind = ui.findComboBox->currentText();

		results.findText = whatFind;

		//这里不能直接修改results.findText的值，该值在外部显示还需要。如果修改则会显示紊乱
		if (m_extend)
		{
			QString extendFind;
			convertExtendedToString(whatFind, extendFind);
			whatFind = extendFind;
		}
		
		//这里的forward一定要是true。回环一定是false
		if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, false, true, FINDNEXTTYPE_FINDNEXT, 0, 0))
		{
			ui.statusbar->showMessage(tr("cant't find text \'%1\'").arg(m_expr), 8000);
			QApplication::beep();
			
			emit sign_findAllInCurDoc(&results);

			m_isFindFirst = true;

			return;
		}
		else
		{
			dealWithZeroFound(pEdit);
		}

		addCurFindRecord(pEdit, results);

		++replaceNums;

		//找到了,把结果收集起来
		while (pEdit->findNext())
		{
			addCurFindRecord(pEdit, results);
			++replaceNums;

			dealWithZeroFound(pEdit);
		}

		pEdit->execute(SCI_GOTOPOS, srcPostion);
		pEdit->execute(SCI_SETFIRSTVISIBLELINE, firstDisLineNum);
		pEdit->execute(SCI_SETXOFFSET, 0);

		//全部替换后，下次查找，必须算第一次查找
		m_isFindFirst = true;
		ui.statusbar->showMessage(tr("find finished, total %1 found!").arg(replaceNums), 10000);

		emit sign_findAllInCurDoc(&results);
	}
	else
	{
		ui.statusbar->showMessage(tr("The mode of the current document does not allow this operation."), 8000);
		QApplication::beep();
	}
}

void FindWin::slot_findAllInOpenDoc()
{
	if (ui.findComboBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}
	
	QString whatFind = ui.findComboBox->currentText();
	QString originWhatFine = whatFind;

	if (m_extend)
	{
		QString extendFind;
		convertExtendedToString(whatFind, extendFind);
		whatFind = extendFind;
	}

	int replaceNums = 0;
	QVector<FindRecords*>* allOpenFileRecord = new QVector<FindRecords*>();

	for (int i = 0; i < m_editTabWidget->count(); ++i)
	{
		QWidget* pw = m_editTabWidget->widget(i);
		ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
		if (pEdit != nullptr)
		{
			if (pEdit->isReadOnly())
			{
				continue;
			}

			FindRecords* results = new FindRecords();
			results->pEdit = pEdit;

			results->findFilePath = pw->property("filePath").toString();

			updateParameterFromUI();

			//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
			//results->findText要是有原来的值，因为扩展模式下\r\n不会转义，直接输出会换行显示
			results->findText = originWhatFine;
			if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, false, true, FINDNEXTTYPE_FINDNEXT, 0, 0))
			{
				delete results;
				continue;
			}
			else
			{
				dealWithZeroFound(pEdit);
			}
			addCurFindRecord(pEdit, *results);

			++replaceNums;

			//找到了,把结果收集起来
			while (pEdit->findNext())
			{
				addCurFindRecord(pEdit, *results);
				++replaceNums;
				dealWithZeroFound(pEdit);
			}

			allOpenFileRecord->append(results);
		}
	}

	//全部替换后，下次查找，必须算第一次查找
	m_isFindFirst = true;
	ui.statusbar->showMessage(tr("find finished, total %1 found!").arg(replaceNums), 10000);

	emit sign_findAllInOpenDoc(allOpenFileRecord, replaceNums, whatFind);

	//释放元素
	for (int i = 0; i < allOpenFileRecord->size(); ++i)
	{
		delete allOpenFileRecord->at(i);
	}

	delete allOpenFileRecord;
	
}

//返回是否查找得到内容
bool FindWin::replaceFindNext(QsciScintilla* pEdit, bool showZeroFindTip)
{
	m_isFound = false;

	//第一次查找
	if (m_isFindFirst)
	{

		if (pEdit != nullptr)
		{
			QString whatFind = ui.replaceTextBox->currentText();

			addFindHistory(whatFind);

			if (m_extend)
			{
				QString extendFind;
				convertExtendedToString(whatFind, extendFind);
				whatFind = extendFind;
			}

			if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, m_wrap, m_forward, FINDNEXTTYPE_REPLACENEXT))
			{
				ui.statusbar->showMessage(tr("cant't find text \'%1\'").arg(m_expr), 8000);
				QApplication::beep();
				m_isFindFirst = true;
			}
			else
			{
				m_isFound = true;
				m_isFindFirst = false;
				dealWithZeroFoundShowTip(pEdit, showZeroFindTip);
			}
		}
	}
	else
	{
		//查找下一个
		if (pEdit != nullptr)
		{
			if (!pEdit->findNext())
			{
				ui.statusbar->showMessage(tr("no more find text \'%1\'").arg(m_expr), 8000);
				m_isFindFirst = true;
				QApplication::beep();
			}
			else
			{
				m_isFound = true;
				dealWithZeroFoundShowTip(pEdit, showZeroFindTip);
			}
		}
	}

	return m_isFound;
}

void FindWin::slot_replaceFindNext()
{
	if (ui.replaceTextBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}

	updateParameterFromUI();

	QWidget* pw = autoAdjustCurrentEditWin();

	QsciScintilla* pEdit = dynamic_cast<QsciScintilla*>(pw);

	replaceFindNext(pEdit, true);
}

//返回值：是否还可以继续替换
bool FindWin::replace(ScintillaEditView* pEdit)
{
	if (isFirstFind())
	{
		replaceFindNext(pEdit, false);
		//如果没有找到，则不替换
		if (!m_isFound)
		{
			return false;
		}
	}

	QString findText = ui.replaceTextBox->currentText();
	QString replaceText = ui.replaceWithBox->currentText();

	if (m_extend)
	{
		QString extendFind;
		convertExtendedToString(findText, extendFind);
		findText = extendFind;

		QString extendReplace;
		convertExtendedToString(replaceText, extendReplace);
		replaceText = extendReplace;
	}

	//当前有选中内容，而且与待替换内容一样，则直接替换
	if (m_isFound && pEdit->hasSelectedText())
	{
		//如果选中内容，与待查找替换的内容一致，进行替换。前提是在normal模式下
		if ((m_searchMode == 1) && (pEdit->selectedText().compare(findText, (m_cs ? Qt::CaseSensitive : Qt::CaseInsensitive)) == 0))
		{
			pEdit->replace(replaceText);
			return replaceFindNext(pEdit,false);
		}
		else if (m_searchMode == 2)
		{
			//如果是正则表达式模式，则不能使用全部匹配才替换，要使用正则匹配，那样会比较麻烦
			//只有上次查找成功，才替换？
			if (m_isFound)
			{
				pEdit->replace(replaceText);
				return replaceFindNext(pEdit,false);
			}
			else
			{
				ui.statusbar->showMessage(tr("no more replace text \'%1\'").arg(m_expr), 8000);
				QApplication::beep();
				return false;
			}
		}
		else
		{
			ui.statusbar->showMessage(tr("no more replace text \'%1\'").arg(m_expr), 8000);
			QApplication::beep();
			return false;
		}
	}
	else if (m_isFound && m_searchMode == 2)
	{
		//找到了内容，但是因为是0长，而无法选中。这种情况就是0长的情况。只在正则表达式情况出现
		pEdit->replace(replaceText);

		//每次替换后，因为是0长替换，再把下次查找位置加1，否则会一直在原地查找
		dealWithZeroFound(pEdit);

		return replaceFindNext(pEdit,false);
	}

		//当前没有查找到
	return replaceFindNext(pEdit,false);
}

//把当前选中的内容，使用文本替换掉
void FindWin::slot_replace()
{

	if (ui.replaceTextBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}

	QWidget* pw = autoAdjustCurrentEditWin();

	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		if (pEdit->isReadOnly())
		{
			ui.statusbar->showMessage(tr("The ReadOnly document does not allow replacement."), 8000);
			QApplication::beep();
			return;
		}

		//切换查询条件后，则是第一次查找。防止前后查找条件发生了变化，
		//导致选中的内容不是需要替换的，所以检查是第一次查找，则查找一下
		updateParameterFromUI();

		replace(pEdit);
	}
	else
	{
		ui.statusbar->showMessage(tr("The mode of the current document does not allow replacement."), 8000);
		QApplication::beep();
	}
}

void FindWin::slot_findModeRegularBtChange(bool checked)
{
	if (checked)
	{
		ui.findBackwardBox->setEnabled(false);
		ui.findBackwardBox->setChecked(false);
		ui.findMatchWholeBox->setEnabled(false);
		ui.findMatchWholeBox->setChecked(false);
	}
	else
	{
		ui.findBackwardBox->setEnabled(true);
		ui.findMatchWholeBox->setEnabled(true);
	}
	m_isFindFirst = true;
}

void FindWin::slot_replaceModeRegularBtChange(bool checked)
{
	if (checked)
	{
		ui.replaceBackwardBox->setEnabled(false);
		ui.replaceBackwardBox->setChecked(false);
		ui.replaceMatchWholeBox->setEnabled(false);
		ui.replaceMatchWholeBox->setChecked(false);
	}
	else
	{
		ui.replaceBackwardBox->setEnabled(true);
		ui.replaceMatchWholeBox->setEnabled(true);
	}
	m_isFindFirst = true;
}


//替换当前文档里面的所有
void FindWin::slot_replaceAll()
{
	if (ui.replaceTextBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		return;
	}

	if (!m_isStatic &&  QMessageBox::Yes != QMessageBox::question(this, tr("Replace All current Doc"), tr("Are you sure replace all occurrences in current documents?")))
	{
		return;
	}

	QWidget* pw = autoAdjustCurrentEditWin();
	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		if (pEdit->isReadOnly())
		{
			ui.statusbar->showMessage(tr("The ReadOnly document does not allow replacement."), 8000);
			QApplication::beep();
			return;
		}

		updateParameterFromUI();

		int srcPostion = pEdit->execute(SCI_GETCURRENTPOS);
		int firstDisLineNum = pEdit->execute(SCI_GETFIRSTVISIBLELINE);

		int replaceNums = 0;

		//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
		QString whatFind = ui.replaceTextBox->currentText();
		QString replaceText = ui.replaceWithBox->currentText();

		if (m_extend)
		{
			QString extendFind;
			convertExtendedToString(whatFind, extendFind);
			whatFind = extendFind;

			QString extendReplace;
			convertExtendedToString(replaceText, extendReplace);
			replaceText = extendReplace;
		}

#if 0
		if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, false, m_forward, 0, 0))
		{
			ui.statusbar->showMessage(tr("cant't find text \'%1\'").arg(m_expr), 8000);
			QApplication::beep();
			m_isFindFirst = true;
			return;
		}
		else
		{
			m_isFound = true;
			m_isFindFirst = false;
		}

		//一定不能回环查找
		m_wrap = false;

		while (replace(pEdit))
		{
			++replaceNums;
		}
#endif
		

		if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, false, true, FINDNEXTTYPE_REPLACENEXT, 0,0))
		{
			ui.statusbar->showMessage(tr("cant't find text \'%1\'").arg(m_expr), 8000);
			QApplication::beep();
			m_isFindFirst = true;
			return;
		}
		pEdit->execute(SCI_BEGINUNDOACTION);

		pEdit->replace(replaceText);

		dealWithZeroFound(pEdit);

		++replaceNums;
	
		//找到了，则自动进行全部替换
		while(pEdit->findNext())
		{
			pEdit->replace(replaceText);
			++replaceNums;
			dealWithZeroFound(pEdit);
		}

		pEdit->execute(SCI_ENDUNDOACTION);

		pEdit->execute(SCI_GOTOPOS, srcPostion);
		pEdit->execute(SCI_SETFIRSTVISIBLELINE, firstDisLineNum);
		pEdit->execute(SCI_SETXOFFSET, 0);

		//全部替换后，下次查找，必须算第一次查找
		m_isFindFirst = true;
		ui.statusbar->showMessage(tr("replace finished, total %1 replaced!").arg(replaceNums), 10000);
	}
	else
	{
		ui.statusbar->showMessage(tr("The mode of the current document does not allow replacement."), 8000);
		QApplication::beep();
	}
}


void FindWin::slot_replaceAllInOpenDoc()
{
	if (ui.replaceTextBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}

	if (QMessageBox::Yes != QMessageBox::question(this, tr("Replace All Open Doc"), tr("Are you sure replace all occurrences in all open documents?")))
	{
		return;
	}

	updateParameterFromUI();

	int replaceNums = 0;

	
	QString whatFind = ui.replaceTextBox->currentText();
	QString whatReplace = m_replaceWithText;

	if (m_extend)
	{
		QString extendFind;
		convertExtendedToString(whatFind, extendFind);
		whatFind = extendFind;

		QString extendReplace;
		convertExtendedToString(whatReplace, extendReplace);
		whatReplace = extendReplace;
	}

	for (int i = 0; i < m_editTabWidget->count(); ++i)
	{
		QWidget* pw = m_editTabWidget->widget(i);
		ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
		if (pEdit != nullptr)
		{
			//只读的文档不能替换
			if (pEdit->isReadOnly())
			{
				continue;
			}
#if 0
			m_isFound = false;


			//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
			if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, false, m_forward, 0, 0))
			{
				continue;
			}
			else
			{
				m_isFound = true;
				m_isFindFirst = false;
			}

			//一定不能回环查找
			m_wrap = false;

			while (replace(pEdit))
			{
				++replaceNums;
			}
#endif


			//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
			if (!pEdit->findFirst(whatFind, m_re, m_cs, m_wo, false, true, FINDNEXTTYPE_REPLACENEXT,0, 0))
			{
				continue;
			}

			pEdit->replace(whatReplace);

			dealWithZeroFound(pEdit);

			++replaceNums;

			//找到了，则自动进行全部替换
			while (pEdit->findNext())
			{
				pEdit->replace(whatReplace);
				++replaceNums;
				dealWithZeroFound(pEdit);
			}
		}
	}

	//全部替换后，下次查找，必须算第一次查找
	m_isFindFirst = true;

	ui.statusbar->showMessage(tr("Replace in Opened Files: %1 occurrences were replaced.").arg(replaceNums), 10000);
}


//标记高亮单词
void FindWin::slot_markAll()
{
	if (ui.markTextBox->currentText().isEmpty())
	{
		ui.statusbar->showMessage(tr("what mark is null !"), 8000);
		QApplication::beep();
		return;
	}

	QWidget* pw = autoAdjustCurrentEditWin();
	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		FindRecords *results = new FindRecords;
		results->pEdit = pEdit;

		results->findFilePath = pw->property("filePath").toString();

		updateParameterFromUI();

		int replaceNums = 0;
		//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
		QString whatMark = ui.markTextBox->currentText();
		results->findText = whatMark;

		if (m_extend)
		{
			QString extendFind;
			convertExtendedToString(whatMark, extendFind);
			whatMark = extendFind;
		}

		int srcPostion = pEdit->execute(SCI_GETCURRENTPOS);
		int firstDisLineNum = pEdit->execute(SCI_GETFIRSTVISIBLELINE);

		if (!pEdit->findFirst(whatMark, m_re, m_cs, m_wo, false, true, FINDNEXTTYPE_FINDNEXT, 0, 0))
		{
			ui.statusbar->showMessage(tr("cant't find text \'%1\'").arg(m_expr), 8000);
			QApplication::beep();
			return;
		}
		else
		{
			//不支持零长的高亮。0长不高亮
			FindState& state = pEdit->getLastFindState();
			if (state.targstart == state.targend)
			{
				ui.statusbar->showMessage(tr("cant't mark text \'%1\'").arg(m_expr), 8000);
				QApplication::beep();
				return;
			}
		}

		addCurFindRecord(pEdit, *results,true);

		++replaceNums;

		//找到了,把结果收集起来
		while (pEdit->findNext())
		{
			addCurFindRecord(pEdit, *results, true);
			++replaceNums;
		}

		//把结果高亮起来。
		int foundTextLen = 0;
		for (int i = 0, s = results->records.size(); i < s; ++i)
		{
			const FindRecord& rs = results->records.at(i);
			foundTextLen = rs.end - rs.pos;

			if (foundTextLen > 0)
			{
				pEdit->execute(SCI_SETINDICATORCURRENT, SCE_UNIVERSAL_FOUND_STYLE);
				pEdit->execute(SCI_INDICATORFILLRANGE, rs.pos, foundTextLen);
			}
		}

		if (!results->records.isEmpty())
		{
			pEdit->appendMarkRecord(results);
		}


		pEdit->execute(SCI_SETFIRSTVISIBLELINE, firstDisLineNum);
		pEdit->execute(SCI_GOTOPOS, srcPostion);
		pEdit->execute(SCI_SETXOFFSET, 0);

		//全部替换后，下次查找，必须算第一次查找
		m_isFindFirst = true;
		ui.statusbar->showMessage(tr("mark finished, total %1 found!").arg(replaceNums), 10000);
	}
	else
	{
		ui.statusbar->showMessage(tr("The mode of the current document does not allow mark."), 8000);
		QApplication::beep();
	}
}

//取消高亮
void FindWin::slot_clearMark()
{
	QWidget* pw = autoAdjustCurrentEditWin();
	ScintillaEditView* pEdit = dynamic_cast<ScintillaEditView*>(pw);
	if (pEdit != nullptr)
	{
		int docEnd = pEdit->length();
		pEdit->execute(SCI_SETINDICATORCURRENT, SCE_UNIVERSAL_FOUND_STYLE);
		pEdit->execute(SCI_INDICATORCLEARRANGE, 0, docEnd);

		pEdit->releaseAllMark();
	}
}

//选择查找目录
void FindWin::slot_dirSelectDest()
{
	QString destDir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), CCNotePad::s_lastOpenDirPath, QFileDialog::DontResolveSymlinks);
	if (!destDir.isEmpty())
	{
		ui.destFindDir->setText(destDir);
	}
}

//在walkDirfile中用作回调函数处理。命中则返回true 
bool FindWin::findTextInFile(QString &filePath, int &findNums, QVector<FindRecords*>* allfileInDirRecord)
{
	pEditTemp->clear();
	
	if (0 != FileManager::getInstance().loadFileForSearch(pEditTemp, filePath))
	{
		return false;
	}

	FindRecords* results = new FindRecords();
	//返回结果的edit无条件写无。注意不要忘记
	results->pEdit = nullptr;
	results->findFilePath = filePath;

	//无条件进行第一次查找，从0行0列开始查找，而且不回环。如果没有找到，则替换完毕
	results->findText = m_expr;
	QString whatFind = m_expr;

	if (m_extend)
	{
		QString extendFind;
		convertExtendedToString(whatFind, extendFind);
		whatFind = extendFind;
	}

	if (!pEditTemp->findFirst(whatFind, m_re, m_cs, m_wo, false, m_forward, FINDNEXTTYPE_FINDNEXT, 0, 0))
	{
		delete results;
		return false;
	}
	else
	{
		dealWithZeroFound(pEditTemp);
	}

	addCurFindRecord(pEditTemp, *results);

	++findNums;

		//找到了,把结果收集起来
	while (pEditTemp->findNext())
	{
		addCurFindRecord(pEditTemp, *results);
		++findNums;
		dealWithZeroFound(pEditTemp);
	}

	allfileInDirRecord->append(results);

	return true;
}

//在walkDirfile中用作回调函数处理。命中则返回true 第三个参数不需要，为了复用walkdir，暂时保留和findTextInFile一致
bool FindWin::replaceTextInFile(QString &filePath, int &replaceNums, QVector<FindRecords*>*)
{
	pEditTemp->clear();

	if (0 != FileManager::getInstance().loadFileForSearch(pEditTemp, filePath))
	{
		return false;
	}

	QString find = m_expr;
	QString replace = m_replaceWithText;
	if (m_extend)
	{
		QString extendFind;
		convertExtendedToString(find, extendFind);
		find = extendFind;

		QString extendReplace;
		convertExtendedToString(replace, extendReplace);
		replace = extendReplace;
	}

	if (!pEditTemp->findFirst(find, m_re, m_cs, m_wo, false, m_forward, FINDNEXTTYPE_REPLACENEXT,0, 0))
	{
		return false;
	}

	pEditTemp->replace(replace);
	dealWithZeroFound(pEditTemp);

	++replaceNums;

	//找到了，则自动进行全部替换
	while (pEditTemp->findNext())
	{
		pEditTemp->replace(replace);
		++replaceNums;
		dealWithZeroFound(pEditTemp);
	}

	//必须要保存一下
	emit sign_replaceSaveFile(filePath, pEditTemp);

	return true;
}

//非递归版本的递归文件，从CompareDirs中修改而来
//isSkipBinary:是否跳过二进制 
//isSkipHide:是否处理隐藏文件
//skipMaxSize::处理文件的最大大小，超过则不处理。如果是0，则表示不跳过任何文件
//isfilterFileType:过滤类型，只处理这类类型文件 为true时 fileExtType 不能为空
//fileExtType的格式为：.cpp .h 类似，但是不需要前面的.，传递的时候不传递下来

int FindWin::walkDirfile(QString path, int &foundTimes, bool isSkipBinary, bool isSkipHide, int skipMaxSize, bool isfilterFileType, QStringList& fileExtType, bool isSkipChildDirs, std::function<bool(QString &, int &, QVector<FindRecords*>* allfileInDirRecord)> foundCallBack, bool isAskAbort)
{
	QList<QString> dirsList;
	QString oneDir(path);
	dirsList.append(oneDir);

	int fileNums = 0;

	int hitFileNums = 0;

	//再获取文件夹到列表
	QDir::Filters dirfilter;

	if (!isSkipHide)
	{
		dirfilter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks;
	}
	else
	{
		dirfilter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;
	}

	//过滤文件类型。true 合格，需要处理， false 不合格，跳过
	auto fileTypeFilter = [&fileExtType](QFileInfo& fileInfo)->bool {
		QString suffix = fileInfo.suffix();
		if (!suffix.isEmpty())
		{
			return (-1 != fileExtType.indexOf(suffix));
		}
		//对于没有后缀的文件，一律跳过
		return false;
	};

	//是否二进制文件
	auto binaryFiltre = [](QFileInfo& fi)->bool {
#if 0
		//如果不支持该文件ext，则以二进制形式打开
		if (DocTypeListView::isSupportExt(fi.suffix()))
		{
			return false;
		}
		else
		{
			static QMimeDatabase db;
			QMimeType mime = db.mimeTypeForFile(fi);
			if (mime.name().startsWith("text")) {
				return false;
			}
		}
		return true;
#endif
		return DocTypeListView::isHexExt(fi.suffix());
	};

	ProgressWin* loadFileProcessWin = new ProgressWin(this);
	
	loadFileProcessWin->setWindowModality(Qt::WindowModal);

	loadFileProcessWin->info(tr("load dir file in progress\n, please wait ..."));

	loadFileProcessWin->show();

	int dirNums = 0;

	bool firstChildDirs = true;
	int totalStep = 0;

	bool canAbort = true;
	bool canAbortSecond = true;

	if (!isAskAbort)
	{
		canAbort = false;
		canAbortSecond = false;
	}

	while (!dirsList.isEmpty())
	{
		QString curDir = dirsList.takeFirst();

		if (!isSkipChildDirs)
		{

		/*添加path路径文件*/
		QDir dir(curDir);          //遍历各级子目录

		QFileInfoList folder_list = dir.entryInfoList(dirfilter);   //获取当前所有目录

		for (int i = 0; i != folder_list.size(); ++i)         //自动递归添加各目录到上一级目录
		{
			QString namepath = folder_list.at(i).absoluteFilePath();    //获取路径
			QFileInfo folderinfo = folder_list.at(i);

			if (folderinfo.baseName().isEmpty())
			{
				loadFileProcessWin->info(tr("skip dir %1").arg(namepath));
				continue;
			}

			QString name = folderinfo.fileName();      //获取目录名

			dirsList.push_front(namepath);

			dirNums++;

			loadFileProcessWin->info(tr("found %1 dir %2").arg(dirNums).arg(namepath));
			QCoreApplication::processEvents(/*QEventLoop::ExcludeUserInputEvents*/);
		}

		if (firstChildDirs)
		{
			totalStep = dirNums;
			loadFileProcessWin->setTotalSteps(dirNums);
			firstChildDirs = false;
		}

		if (dirsList.size() < totalStep)
		{
			totalStep = dirsList.size();
			loadFileProcessWin->moveStep();
		}
		}

		QDir dir_file(curDir);

		if (!isSkipHide)
		{
			dir_file.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks);//获取当前所有文件
		}
		else
		{
			dir_file.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);//获取当前所有文件
		}

		QFileInfoList list_file = dir_file.entryInfoList();
		for (int i = 0; i < list_file.size(); ++i)
		{  //将当前目录中所有文件添加到treewidget中
			QFileInfo fileInfo = list_file.at(i);
			QString namepath = list_file.at(i).absoluteFilePath();    //获取路径

			//不支持所有文件，仅仅支持指定类型文件。没有通过
			if (isfilterFileType && !fileTypeFilter(fileInfo))
			{
				loadFileProcessWin->info(tr("ext type  skip file %1").arg(namepath));
				continue;
			}

			//大小过滤
			if ((skipMaxSize != 0) && fileInfo.size() > skipMaxSize)
			{
				continue;
			}
			
			//二进制过滤//对于二进制文件该如何处理，我觉得必须要过滤，暂时不处理
			if (isSkipBinary &&binaryFiltre(fileInfo))
			{
				continue;
			}

			//回调处理该函数
			if (foundCallBack(namepath, foundTimes, nullptr))
			{
				++hitFileNums;
			}

			if (i % 2 == 0)
			{
				if (loadFileProcessWin->isCancel())
				{
					loadFileProcessWin->info(tr("found in dir canceled ..."));
					break;
				}
				QCoreApplication::processEvents();
			}

			if ((canAbort && (hitFileNums > 100 || foundTimes > 1000)) || (canAbortSecond && foundTimes > 8000))
			{
				int ret = QMessageBox::question(this, tr("Continue Find ?"), tr("The search results have been greater than %1 times in %2 files, and more may be slow. Continue to search?").arg(foundTimes).arg(hitFileNums), tr("Yes"), tr("Abort"));
				if(ret == 1)
				{
					loadFileProcessWin->setCancel();
					break;
		}
				else
				{
					if (canAbort && (hitFileNums > 100 || foundTimes > 1000))
					{
						canAbort = false;
					}
					if ((canAbortSecond && foundTimes > 8000))
					{
						canAbortSecond = false;
					}
				}
			}

		}

		fileNums += list_file.size();

		if (loadFileProcessWin->isCancel())
		{
			break;
		}
	}

	if (loadFileProcessWin != nullptr)
	{
		delete loadFileProcessWin;
		loadFileProcessWin = nullptr;
	}

	return fileNums;
}


//在目标文件夹中查找
void FindWin::slot_dirFindAll()
{
	QString dirPath = ui.destFindDir->text().trimmed();
	QString whatFind = ui.dirFindWhat->currentText();

	if (dirPath.isEmpty())
	{
		ui.statusbar->showMessage(tr("please select find dest dir !"), 8000);
		QApplication::beep();
		return;
	}

	if(whatFind.isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}

	QDir dir(dirPath);
	if (!dir.exists())
	{
		ui.statusbar->showMessage(tr("dest dir %1 not exist !").arg(dirPath), 8000);
		QApplication::beep();
		return;
	}

	bool isfilterFileType = ui.dealFileType->isChecked();
	QStringList fileExtTypeList;

	if (isfilterFileType)
	{
		QString fileExtType = ui.fileType->text().trimmed();
		if (fileExtType.isEmpty() || (fileExtType == "*.*"))
		{
			isfilterFileType = false;
		}
		else
		{
			//格式是*.h:*.c:*.cpp类似的
			QStringList typeList = fileExtType.split(":");
			foreach (QString var, typeList)
			{
				if (var.size() >= 3)
				{
					//只取后面的h或或cpp后缀
					fileExtTypeList.append(var.mid(2));
				}
			}
			if (fileExtTypeList.isEmpty())
			{
				isfilterFileType = false;
			}
		}
	}

	
	bool isSkipBinary = ui.skipBinary->isChecked();
	bool  isSkipHide = ui.skipHideFile->isChecked();
	int skipMaxSize = (ui.skipFileMaxSize->isChecked()) ? ui.maxFileSizeSpinBox->value()*1024*1024:0;
	bool isSkipChildDir = ui.skipChildDirs->isChecked();

	updateParameterFromUI();

	if (pEditTemp == nullptr)
	{
		pEditTemp = new ScintillaEditView(nullptr);
	}

	int foundNums = 0;
	QVector<FindRecords*>* allfileInDirRecord = new QVector<FindRecords*>();

	std::function<bool(QString &, int &, QVector<FindRecords*>* allfileInDirRecord)> foundCallBack = std::bind(&FindWin::findTextInFile, this, std::placeholders::_1, std::placeholders::_2, allfileInDirRecord);

	int filesNum = walkDirfile(dirPath, foundNums, isSkipBinary, isSkipHide, skipMaxSize, isfilterFileType, fileExtTypeList, isSkipChildDir, foundCallBack);

	//全部替换后，下次查找，必须算第一次查找
	m_isFindFirst = true;
	ui.statusbar->showMessage(tr("find finished, total %1 found in %2 file!").arg(foundNums).arg(filesNum), 10000);

	//复用了这个信号函数，没有新做消息，要注意
	emit sign_findAllInOpenDoc(allfileInDirRecord, foundNums, whatFind);

	addFindHistory(whatFind);

	//释放元素
	for (int i = 0; i < allfileInDirRecord->size(); ++i)
	{
		delete allfileInDirRecord->at(i);
	}

	delete allfileInDirRecord;

}

//目录中直接替换
void FindWin::slot_dirReplaceAll()
{
	QString dirPath = ui.destFindDir->text();
	QString whatFind = ui.dirFindWhat->currentText();
	QString dirReplaceWhat = ui.dirReplaceWhat->currentText();

	if (dirPath.isEmpty())
	{
		ui.statusbar->showMessage(tr("please select find dest dir !"), 8000);
		QApplication::beep();
		return;
}

	if (whatFind.isEmpty())
	{
		ui.statusbar->showMessage(tr("what find is null !"), 8000);
		QApplication::beep();
		return;
	}

	if (QMessageBox::Yes != QMessageBox::question(this, tr("Replace All Dirs"), tr("Are you sure replace all \"%1\" to \"%2\" occurrences in selected dirs ?").arg(whatFind).arg(dirReplaceWhat)))
	{
		return;
	}

	bool isfilterFileType = ui.dealFileType->isChecked();
	QStringList fileExtTypeList;

	if (isfilterFileType)
	{
		QString fileExtType = ui.fileType->text().trimmed();
		if (fileExtType.isEmpty() || (fileExtType == "*.*"))
		{
			isfilterFileType = false;
		}
		else
		{
			//格式是*.h:*.c:*.cpp类似的
			QStringList typeList = fileExtType.split(":");
			foreach(QString var, typeList)
			{
				if (var.size() >= 3)
				{
					//只取后面的h或或cpp后缀
					fileExtTypeList.append(var.mid(2));
				}
			}
			if (fileExtTypeList.isEmpty())
			{
				isfilterFileType = false;
			}
		}
	}


	bool isSkipBinary = ui.skipBinary->isChecked();
	bool  isSkipHide = ui.skipHideFile->isChecked();
	int skipMaxSize = (ui.skipFileMaxSize->isChecked()) ? ui.maxFileSizeSpinBox->value() * 1024 * 1024 : 0;
	bool isSkipChildDir = ui.skipChildDirs->isChecked();

	updateParameterFromUI();

	if (pEditTemp == nullptr)
	{
		pEditTemp = new ScintillaEditView(nullptr);
	}
	int replaceNums = 0;

	std::function<bool(QString &, int &, QVector<FindRecords*>* allfileInDirRecord)> foundCallBack = std::bind(&FindWin::replaceTextInFile, this, std::placeholders::_1, std::placeholders::_2, nullptr);

	int filesNum = walkDirfile(dirPath, replaceNums, isSkipBinary, isSkipHide, skipMaxSize, isfilterFileType, fileExtTypeList, isSkipChildDir, foundCallBack,false);

	//全部替换后，下次查找，必须算第一次查找
	m_isFindFirst = true;
	ui.statusbar->showMessage(tr("replace finished, total %1 replace in %2 file!").arg(replaceNums).arg(filesNum), 10000);
}
