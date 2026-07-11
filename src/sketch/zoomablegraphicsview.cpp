/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2019 Fritzing

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#include <QWheelEvent>
#include <QScrollBar>
#include <QSettings>
#include <QGestureEvent>
#include <QInputDevice>

#include "zoomablegraphicsview.h"
#include "../utils/zoomslider.h"
#include "../utils/misc.h"
#include "debugdialog.h"

const int ZoomableGraphicsView::MaxScaleValue = 3000;


ZoomableGraphicsView::WheelMapping ZoomableGraphicsView::m_wheelMapping =
#if defined(Q_OS_LINUX)
    Guess;
#elif defined(Q_OS_MACOS)
    Pure;
#elif defined(Q_OS_WINDOWS) || defined(Q_OS_WIN)
    ZoomPrimary;
#else
    ScrollPrimary;
#endif

bool FirstTime = true;

ZoomableGraphicsView::ZoomableGraphicsView( QWidget * parent )
	: QGraphicsView(parent)
{
	m_viewFromBelow = false;
	m_scaleValue = 100;
	m_maxScaleValue = MaxScaleValue;
	m_minScaleValue = 1;
	m_acceptWheelEvents = true;
	m_guessTouchpadId = -1;
	if (FirstTime) {
		FirstTime = false;
		QSettings settings;
		m_wheelMapping = (WheelMapping) settings.value("wheelMapping", m_wheelMapping).toInt();
		if (m_wheelMapping >= WheelMappingCount) {
#if defined(Q_OS_LINUX)
			m_wheelMapping = Guess;
#elif defined(Q_OS_MACOS)
			m_wheelMapping = Pure;
#elif defined(Q_OS_WINDOWS) || defined(Q_OS_WIN)
			m_wheelMapping = ZoomPrimary;
#else
			m_wheelMapping = ScrollPrimary;
#endif
		}
	}
	grabGesture(Qt::PinchGesture);
	viewport()->grabGesture(Qt::PinchGesture);
	viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
}

bool ZoomableGraphicsView::event(QEvent *event) {
	if (event->type() == QEvent::Gesture)
		return gestureEvent(static_cast<QGestureEvent*>(event));
	return QGraphicsView::event(event);
}

bool ZoomableGraphicsView::viewportEvent(QEvent *event) {
	if (event->type() == QEvent::Gesture) {
		return gestureEvent(static_cast<QGestureEvent*>(event));
	}
	return QGraphicsView::viewportEvent(event);
}

void ZoomableGraphicsView::wheelEvent(QWheelEvent* event) {
	qint64 systemId =  event->device()->systemId();

	if (!m_acceptWheelEvents) {
		QGraphicsView::wheelEvent(event);
		return;
	}
	bool doZoom = false;
	bool doHorizontal = false;
	bool doVertical = false;
	bool do2d = false;
	bool dampen = true;

	bool control = event->modifiers() & Qt::ControlModifier;
	bool alt = event->modifiers() & altOrMetaModifier();
	bool shift = event->modifiers() & Qt::ShiftModifier;

	switch (m_wheelMapping) {
	case ScrollPrimary:
//		qDebug() << "scroll primary";
		doZoom = (control || alt);
		if (!doZoom) {
			if (event->angleDelta().x() != 0 && event->angleDelta().y() == 0) {
				doHorizontal = true;
				doVertical = false;
			} else {
				doVertical = !shift;
				doHorizontal = shift;
			}
		} else {
			dampen = shift;
		}
		break;
	case ZoomPrimary:
//		qDebug() << "zoom primary";
		doZoom = !(control || alt);
		if (!doZoom) {
			if (event->angleDelta().x() != 0 && event->angleDelta().y() == 0) {
				doHorizontal = true;
				doVertical = false;
			} else {
				doVertical = !shift;
				doHorizontal = shift;
			}
		} else {
			dampen = shift;
		}
		break;
	case Guess:
//		qDebug() << "guess";
		// Set things up for a touchpad
		do2d = !(control || alt);
		doZoom = (control || alt);
		dampen = shift;
		if (systemId == m_guessTouchpadId) break; // We already detected a touchpad
		
		// If the device is explicitly a touchpad, lock to it immediately!
		if (event->device() && event->device()->type() == QInputDevice::DeviceType::TouchPad) {
			m_guessTouchpadId = systemId;
			break;
		}

		// TODO: What about "wheels" with a left and right button?
		// The 'buttons' are ignored, but maybe some special wheels report this as an axis?
		if (event->angleDelta().x() != 0) {
			DebugDialog::debug(QString("got lock on touchpad, it moved horizontal. %1").arg(systemId));
			m_guessTouchpadId = systemId;
			break;
		}
		if (abs(event->angleDelta().y()) < 80) {
			DebugDialog::debug(QString("got lock on touchpad, it did a small move").arg(systemId));
			// TODO: Are there high resolution wheels that go below 80 ? Standard seems 120
			// record data with 'sudo libinput debug-events' to check for possible improvements.
			m_guessTouchpadId = systemId;
			break;
		}
		// Probably a real wheel, treat similar to "zoom primary" (on Windows) or "scroll primary" (on Linux/Mac)
		do2d = false;
#if defined(Q_OS_WIN) || defined(Q_OS_WINDOWS)
		doZoom = !(control || alt);
		if (!doZoom) {
			if (event->angleDelta().x() != 0 && event->angleDelta().y() == 0) {
				doHorizontal = true;
				doVertical = false;
			} else {
				doVertical = !shift;
				doHorizontal = shift;
			}
			dampen = true;
		}
#else
		doZoom = (control || alt);
		if (!doZoom) {
			if (event->angleDelta().x() != 0 && event->angleDelta().y() == 0) {
				doHorizontal = true;
				doVertical = false;
			} else {
				doVertical = !shift;
				doHorizontal = shift;
			}
		} else {
			dampen = shift;
		}
#endif
		break;
	case Pure:
//		qDebug() << "pure";
		QGraphicsView::wheelEvent(event);
		return;
	default:
		DebugDialog::debug("Error: Invalid wheel mapping.");
		QGraphicsView::wheelEvent(event);
		return;
	}

	int numSteps = event->angleDelta().y();
	if (doHorizontal && event->angleDelta().x() != 0 && event->angleDelta().y() == 0) {
		numSteps = event->angleDelta().x();
	}
	if (dampen) {
//		qDebug() << "dampen";
		numSteps /= 8;
	}
//	qDebug() << "numSteps" << numSteps;

	if (doZoom) {
//		qDebug() << "doZoom";
		double delta = ((double) numSteps / 120) * ZoomSlider::ZoomStep;
		if (delta == 0) return;
//		qDebug() << "delta " << delta;
		// Scroll zooming relative to the current size
		relativeZoom(2*delta, true);
		if (doVertical || doHorizontal || do2d) {
			DebugDialog::debug("Error: unexpected during zoom.");
		}
	}
	if (doVertical) {
//		qDebug() << "vertical " << numSteps;
		verticalScrollBar()->setValue( verticalScrollBar()->value() - numSteps);
		if (doZoom || doHorizontal || do2d) {
			DebugDialog::debug("Error: unexpected during vertical scroll.");
		}
	}
	if (doHorizontal) {
//		qDebug() << "horizontal " << numSteps;
		horizontalScrollBar()->setValue( horizontalScrollBar()->value() - numSteps);
		if (doZoom || doVertical || do2d) {
			DebugDialog::debug("Error: unexpected during horizontal scroll.");
		}
	}
	if (do2d) {
//		qDebug() << "2d " << event->angleDelta();
		verticalScrollBar()->setValue( verticalScrollBar()->value() - numSteps);
		numSteps = event->angleDelta().x();
		if (dampen) numSteps /= 8;
		horizontalScrollBar()->setValue( horizontalScrollBar()->value() - numSteps);
		if (doZoom || doVertical || doHorizontal) {
			DebugDialog::debug("Error: unexpected during combined pan+scroll.");
		}
	}
	if (!(do2d || doZoom || doVertical || doHorizontal)) {
		DebugDialog::debug("Error: action expected.");
	}

}


void ZoomableGraphicsView::relativeZoom(double step, bool centerOnCursor) {
	double tempSize = m_scaleValue + step;
	if (tempSize < m_minScaleValue) {
		m_scaleValue = m_minScaleValue;
		Q_EMIT zoomOutOfRange(m_scaleValue);
		return;
	}
	if (tempSize > m_maxScaleValue) {
		m_scaleValue = m_maxScaleValue;
		Q_EMIT zoomOutOfRange(m_scaleValue);
		return;
	}
	double tempScaleValue = tempSize/100;

	m_scaleValue = tempSize;

	//QPoint p = QCursor::pos();
	//QPoint q = this->mapFromGlobal(p);
	//QPointF r = this->mapToScene(q);

	QTransform transform;
	double multiplier = 1;
	if (m_viewFromBelow) multiplier = -1;
	transform.scale(multiplier * tempScaleValue, tempScaleValue);
	if (centerOnCursor) {
		//this->setTransform(QTransform().translate(-r.x(), -r.y()) * transform * QTransform().translate(r.x(), r.y()));
		this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	}
	else {
		this->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
	}
	this->setTransform(transform);

	Q_EMIT zoomChanged(m_scaleValue);
}

void ZoomableGraphicsView::absoluteZoom(double percent) {
	relativeZoom(percent-m_scaleValue, false);
}

double ZoomableGraphicsView::currentZoom() {
	return m_scaleValue;
}

void ZoomableGraphicsView::setAcceptWheelEvents(bool accept) {
	m_acceptWheelEvents = accept;
}

void ZoomableGraphicsView::setWheelMapping(WheelMapping wm) {
	m_wheelMapping = wm;
}

ZoomableGraphicsView::WheelMapping ZoomableGraphicsView::wheelMapping() {
	return m_wheelMapping;
}

bool ZoomableGraphicsView::viewFromBelow() {
	return m_viewFromBelow;
}

void ZoomableGraphicsView::setViewFromBelow(bool viewFromBelow) {
	if (m_viewFromBelow == viewFromBelow) return;

	m_viewFromBelow = viewFromBelow;

	this->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
	QTransform transform;
	transform.scale(-1, 1);
	this->setTransform(transform, true);
}

bool ZoomableGraphicsView::gestureEvent(QGestureEvent *event) {
	if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
		pinchTriggered(static_cast<QPinchGesture *>(pinch));
		event->accept(Qt::PinchGesture);
		return true;
	}
	return false;
}

void ZoomableGraphicsView::pinchTriggered(QPinchGesture *gesture) {
	QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
	QString result;
	if (changeFlags & QPinchGesture::ScaleFactorChanged) {
		qreal zoomFactor = gesture->scaleFactor();
		result = "Zoom event with scale factor: " + QString::number(zoomFactor);
		qreal step = (m_scaleValue * zoomFactor) - m_scaleValue;
		relativeZoom(step, true);
		DebugDialog::debug(result);
	}
}
