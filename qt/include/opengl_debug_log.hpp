#ifndef VISUALIZATION_QT_INCLUDE_OPENGL_DEBUG_LOG_HPP_
#define VISUALIZATION_QT_INCLUDE_OPENGL_DEBUG_LOG_HPP_

#include <QWidget>
#include <QPushButton>
#include <QPlainTextEdit>

#include <QOpenGLDebugMessage>

class OpenGLDebugLog : public QWidget {
	public:
		OpenGLDebugLog(void);
		~OpenGLDebugLog(void);
	public slots:
		void addOpenGLMessage(QOpenGLDebugMessage _m);
	signals:
		/// @b Signals a new message has been generated by the GL log.
		void newMessageGenerated(void);
	protected:
		void setupWidgets();
		void setupSignals();
	protected:
		QLayout* layout;
		QPlainTextEdit* messageOutput;
};

#endif // VISUALIZATION_QT_INCLUDE_OPENGL_DEBUG_LOG_HPP_