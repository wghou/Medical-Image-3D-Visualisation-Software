#ifndef RECTANGLESELECTION_H
#define RECTANGLESELECTION_H

#include <QOpenGLFunctions>
#include <QGLViewer/qglviewer.h>
#include <QtGui>

class RectangleSelection : public QObject {
Q_OBJECT

	// Current rectangular selection
	QRect rectangle_;

	// Different selection modes
	enum SelectionMode { INACTIVE , ACTIVE , ADD_FIXED , ADD_MOVING , REMOVE };
	SelectionMode selectionMode_;

public:

	RectangleSelection() : selectionMode_( INACTIVE ) {}

	bool isInactive()
	{
		return ( this->selectionMode_ == INACTIVE );
	}

	void deactivate()
	{
		this->selectionMode_ = INACTIVE;
	}

	void mousePressEvent(QMouseEvent* e , qglviewer::Camera* const)
	{
		if( this->selectionMode_ == INACTIVE )
			return;

		// Start selection. Mode is ADD by default and REMOVE with Ctrl key.
		rectangle_ = QRect(e->pos(), e->pos());

		if ((e->button() == Qt::LeftButton) && (e->modifiers() & Qt::ControlModifier))
		{
			selectionMode_ = REMOVE;
		} else if ((e->button() == Qt::LeftButton) && (e->modifiers() & Qt::AltModifier))
		{
			selectionMode_ = ADD_FIXED;
		}
		else if ((e->button() == Qt::LeftButton))
		{
			selectionMode_ = ADD_MOVING;
		}
		else if ((e->button() == Qt::RightButton))
		{
			selectionMode_ = INACTIVE;
		}
		Q_EMIT apply();
	}

	void mouseMoveEvent(QMouseEvent* e , qglviewer::Camera* const )
	{
		if(selectionMode_ == ACTIVE)
		{
			rectangle_.setTopLeft(e->pos());
		}
		else if(selectionMode_ != INACTIVE)
		{
			rectangle_.setBottomRight(e->pos());
		}
	}

	void mouseReleaseEvent(QMouseEvent* , qglviewer::Camera* const camera )
	{
		if (selectionMode_ != INACTIVE)
		{
			// Actual selection on the rectangular area.
			// Possibly swap left/right and top/bottom to make rectangle_ valid.
			rectangle_ = rectangle_.normalized();

			QRectF toQ_EMIT(
			  (static_cast<float>(rectangle_.center().x()) - ((float)(rectangle_.width())/2.f))/ static_cast<float>(camera->screenWidth()) ,
			  (static_cast<float>(rectangle_.center().y()) - ((float)(rectangle_.height())/2.f))/ static_cast<float>(camera->screenHeight()) ,
			  (float)(rectangle_.width())/ static_cast<float>(camera->screenWidth()) ,
			  (float)(rectangle_.height())/ static_cast<float>(camera->screenHeight())
			);

			// Use rectangle_.width() , rectangle_.height() , rectangle_.center() to Q_EMIT the signal you want
			if( this->selectionMode_ == ADD_MOVING )
				Q_EMIT add( toQ_EMIT , true );
			if( this->selectionMode_ == ADD_FIXED )
				Q_EMIT add( toQ_EMIT , false );
			else if( this->selectionMode_ == REMOVE )
				Q_EMIT remove( toQ_EMIT );

			this->selectionMode_ = ACTIVE;
		}
	}

	void activate()
	{
		this->selectionMode_ = ACTIVE;
	}

	void draw() const
	{
		if( this->selectionMode_ == INACTIVE || this->selectionMode_ == ACTIVE )
			return;

		bool enable_clip = glIsEnabled(GL_CLIP_PLANE0);
		if (enable_clip) { glDisable(GL_CLIP_PLANE0); }

		float viewport[4];
		glGetFloatv( GL_VIEWPORT , viewport );

		float w = viewport[2] , h = viewport[3];
		float left = (float)(rectangle_.left()) / w;
		float right = (float)(rectangle_.right()) / w;
		float top = 1.f - (float)(rectangle_.top()) / h;
		float bottom = 1.f - (float)(rectangle_.bottom()) / h;

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glEnable(GL_BLEND);

		float f;
		glGetFloatv(GL_LINE_WIDTH, &f);
		GLint poly_mode;
		glGetIntegerv(GL_POLYGON_MODE, &poly_mode);
		glLineWidth(2.f);
		glPolygonMode( GL_FRONT_AND_BACK , GL_LINE );

		glMatrixMode( GL_PROJECTION );
		glPushMatrix();
		glLoadIdentity();
		glOrtho( 0.f , 1.f , 0.f , 1.f , -1.f , 1.f );
		glMatrixMode( GL_MODELVIEW );
		glPushMatrix();
		glLoadIdentity();

		if( this->selectionMode_ == ADD_FIXED || this->selectionMode_ == ADD_MOVING )
			glColor4f(0.2, 0.2, 0.8f , 0.3f);
		if( this->selectionMode_ == REMOVE )
			glColor4f(0.8, 0.2, 0.2f , 0.3f);

		glBegin(GL_QUADS);
		glVertex2f( left , top );
		glVertex2f( left , bottom );
		glVertex2f( right , bottom );
		glVertex2f( right , top );
		glEnd();

		glLineWidth(2.0);
		if( this->selectionMode_ == ADD_FIXED || this->selectionMode_ == ADD_MOVING )
			glColor4f(0.1, 0.1, 1.f , 0.5f);
		if( this->selectionMode_ == REMOVE )
			glColor4f(0.9, 0.1, 0.1f , 0.5f);
		glBegin(GL_LINE_LOOP);
		glVertex2f( left , top );
		glVertex2f( left , bottom );
		glVertex2f( right , bottom );
		glVertex2f( right , top );
		glEnd();

		glLineWidth(f);
		glPolygonMode(GL_FRONT_AND_BACK, poly_mode);

		glPopMatrix();
		glMatrixMode( GL_PROJECTION );
		glPopMatrix();
		glMatrixMode( GL_MODELVIEW );

		glDisable(GL_BLEND);
		glEnable(GL_LIGHTING);
		glEnable(GL_DEPTH_TEST);

		if (enable_clip) { glEnable( GL_CLIP_PLANE0 ); }
	}

public Q_SLOTS:

Q_SIGNALS:
	void add( QRectF rectangle , bool );
	void remove( QRectF rectangle );
	void apply();
};

#endif // RECTANGLESELECTION_H
