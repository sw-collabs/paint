//
// Created by Patricia M Marukot on 2020-04-30.
//
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "canvas.h"
#include "interpolation.h"

#include <stdio.h>
#include <iostream>

#define LOC(x,y,w) (3*((y)*(w)+(x)))

BEGIN_EVENT_TABLE( Canvas, wxPanel )
  EVT_PAINT(Canvas::paintEvent)
  EVT_MOTION(Canvas::mouseMoved)
  EVT_LEFT_DOWN(Canvas::mouseDown)
  EVT_LEFT_UP(Canvas::mouseReleased)
END_EVENT_TABLE()

/* CONSTRUCTORS */
Canvas::Canvas(wxFrame *parent) :
wxPanel(parent) {
  color.set(0, 0, 0);
}

Canvas::Canvas(wxFrame *parent, unsigned int width, unsigned int height) :
wxPanel(parent) {
  this->width = width;
  this->height = height;

  color.set(0, 0, 0);

  /* Initialize the buffer */
  size_t sz = 3*width*height*sizeof(char);
  Buffer = (char *)malloc(sz);

  /* White-out buffer */
  memset(Buffer, 255, sz);
}

void Canvas::addTransaction(Transaction &t) {
  transactions.push_back(&t);
}

void Canvas::updateBuffer(const Pixel &p) {
  /* Update buffer with new colors */
  int i = LOC(p.x, p.y, width);
  Buffer[i] = p.color.r;
  Buffer[i+1] = p.color.g;
  Buffer[i+2] = p.color.b;
}

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */
void Canvas::paintEvent(wxPaintEvent & evt)
{
  wxPaintDC dc(this);
  render(dc);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void Canvas::render(wxDC&  dc)
{
  /*
   * Draw out the bitmap
   */
  
  ////////////////////////////////////
  int i,j;
  i=height/2;
  for (j=0; j<width; j++) {
    Pixel p(0, 0, 0, j, i);
    updateBuffer(p);
  }

  wxImage img(width, height, (unsigned char *)Buffer, true);
  wxBitmap bmp(img);  
  dc.DrawBitmap(bmp, 0, 0, false);
  ///////////////////////////////////
}

/* Event handlers to handle CANVAS mouse events */
/*
 * Handle CLICK event
 * Drawing Tools: Pencil, Line, DrawRect, DrawCircle
 *    - Fill in the clicked pixel
 */
void Canvas::mouseDown(wxMouseEvent &evt)
{
  int x = evt.GetX();
  int y = evt.GetY();
  prevCoord = wxPoint(x, y);

  Pixel p(color.r, color.g, color.b, x, y);
  switch(toolType) {
    case Pencil:
    case Line:
    case DrawRect:
    case DrawCircle:
      updateBuffer(p);
      break;
    case Eraser:
      break;
    case Fill:
      break;
    case SlctRect:
      break;
    case SlctCircle:
      break;
    case Lasso:
      break;
    default:
      break;
  }
  wxWindow::Refresh();
}

void Canvas::mouseMoved(wxMouseEvent &evt)
{
  if (!evt.LeftIsDown())
    return;

  int x=evt.GetX();
  int y=evt.GetY();
  std::vector<wxPoint> points =
      linearInterpolation(prevCoord, wxPoint(x, y));

  Pixel p;
  wxPoint point;
  int i;
  switch(toolType) {
    case Pencil:
      for (i=0; i<points.size(); i++) {
        point = points[i];
        p = Pixel(color.r, color.g, color.b, point.x, point.y);
        updateBuffer(p);
      }
    case Line:
    case DrawRect:
    case DrawCircle:
      wxWindow::Refresh();
      break;
    case Eraser:
      break;
    case Fill:
      break;
    case SlctRect:
      break;
    case SlctCircle:
      break;
    case Lasso:
      break;
    default:
      break;
  }

  prevCoord = wxPoint(x, y);
}

void Canvas::mouseReleased(wxMouseEvent &evt)
{
}

