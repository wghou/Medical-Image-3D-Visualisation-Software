#ifndef VISUALIZATION_QT_INCLUDE_OPENGL_DEBUG_LOG_HPP_
#define VISUALIZATION_QT_INCLUDE_OPENGL_DEBUG_LOG_HPP_

#include <QPlainTextEdit>
#include <QPushButton>
#include <QWidget>

#include <QOpenGLDebugMessage>

/// @ingroup qtwidgets
/// @brief The OpenGLDebugLog class represents a simple plaintext widget showing the output of OpenGL messages.
/// @note This might not always be enabled. Most notably, if the OpenGL context does not support the debug extension
/// (GL_KHR_debug), nothing will be popping up in this widget.
class OpenGLDebugLog : public QWidget {
public:
	OpenGLDebugLog(void);
	~OpenGLDebugLog(void);
public slots:
	/// @brief Called whenever the OpenGL context produces a message.
	void addOpenGLMessage(QOpenGLDebugMessage _m);
	/// @brief Called whenever the user wants to generate a message.
	void addErrorMessage(QString _m);
signals:
	/// @brief Signals a new message has been generated by the GL log.
	void newMessageGenerated(void);

protected:
	void setupWidgets();
	void setupSignals();

protected:
	QLayout* layout;	///< the layout of the widget
	QPlainTextEdit* messageOutput;	  ///< the output panel
	std::size_t groupDepth;	   ///< the current group depth (used for indentation)
};

#endif	  // VISUALIZATION_QT_INCLUDE_OPENGL_DEBUG_LOG_HPP_