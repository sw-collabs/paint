//
// Created by Patricia M Marukot on 2020-04-30.
//
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#include <wx/clipbrd.h>
#endif

#include <unordered_set>
#include <queue>
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <math.h>

#include "helper.h"
#include "canvas.h"
#include "interpolation.h"
#include "selection.h"

#define LOC(x,y,w) (3*((y)*(w)+(x)))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

BEGIN_EVENT_TABLE( Canvas, wxPanel )
  EVT_KEY_DOWN(Canvas::keyDownEvent)
  EVT_KEY_UP(Canvas::keyUpEvent)
  EVT_PAINT(Canvas::paintEvent)
  EVT_MOTION(Canvas::mouseMoved)
  EVT_LEFT_DOWN(Canvas::mouseDown)
  EVT_LEFT_UP(Canvas::mouseReleased)
END_EVENT_TABLE()

Color WHITE = Color((char) 255, (char) 255, (char) 255);
Color SELECT = Color((char) 66, (char) 135, (char) 245);

/* CONSTRUCTORS */
Canvas::Canvas(wxFrame *parent) :
wxPanel(parent) {
  color = Color(0, 0, 0);
}

Canvas::Canvas(wxFrame *parent, unsigned int width, unsigned int height) :
wxPanel(parent) {
  this->width = width;
  this->height = height;

  color = Color(0, 0, 0);

  /* Initialize the buffer */
  size_t sz = 3*width*height*sizeof(char);
  Buffer = (char *)malloc(sz);

  /* White-out buffer */
  memset(Buffer, 255, sz);
}

void Canvas::addTransaction(Transaction &t) {
  transactions.push_back(t);
}

void Canvas::revertTransaction(Transaction &txn) {
  std::vector<Pixel> *pixels;
  Pixel p;
  pixels = &(txn.pixels);
  int i;
  for (i=0; i<pixels->size(); i++) {
    p = (*pixels)[i];
    updateBuffer(p); 
  }
}

void
Canvas::updateTransaction(Transaction &txn, const std::vector<wxPoint> &points)
{
  int i=0;
  wxPoint pt;
  Pixel p;
  for (i=0; i < points.size(); i++) {
    pt = points[i];
    p = Pixel(getPixelColor(pt), pt);
    txn.update(p);
  }
}

Color
Canvas::getPixelColor(const wxPoint &p) {
  /* Update buffer with new colors */
  if (p.x >= width || p.y >= height)
    return WHITE;

  int i = LOC(p.x, p.y, width);
  if (i >= 3 * width * height)
    return WHITE;

  return Color(
    Buffer[i],
    Buffer[i+1],
    Buffer[i+2]
  );
}

Pixel
Canvas::getPixel(const wxPoint &p) {
  wxPoint _p(p.x, p.y);
  return Pixel(getPixelColor(p), _p);
}

void
Canvas::getNeighbors(const wxPoint &p, wxPoint *neighbors, int &ncount) {
  int x, y;
  ncount = 0;

  // Up
  x = p.x;
  y = p.y - 1;
  if (x < width && y < height) {
    neighbors[ncount].x = x;
    neighbors[ncount].y = y;
    ncount++; 
  }

  // Down
  x = p.x;
  y = p.y + 1;
  if (x < width && y < height) {
    neighbors[ncount].x = x;
    neighbors[ncount].y = y;
    ncount++; 
  }

  // Left
  x = p.x - 1;
  y = p.y;
  if (x < width && y < height) {
    neighbors[ncount].x = x;
    neighbors[ncount].y = y;
    ncount++; 
  }

  // Right
  x = p.x + 1;
  y = p.y;
  if (x < width && y < height) {
    neighbors[ncount].x = x;
    neighbors[ncount].y = y;
    ncount++; 
  }
}

void Canvas::updateBuffer(const Pixel &p) {
  /* Update buffer with new colors */
  if (p.x >= width || p.y >= height)
    return;

  int i = LOC(p.x, p.y, width);
  if (i >= 3 * width * height || i < 0)
    return;

  Buffer[i] = p.color.r;
  Buffer[i+1] = p.color.g;
  Buffer[i+2] = p.color.b;
}

void Canvas::updateBuffer(const std::vector<wxPoint> &points,
                          const Color &color) {
  Pixel p;
  wxPoint point;
  int i;
  for (i=0; i<points.size(); i++) {
    point = points[i];
    p = Pixel(color.r, color.g, color.b, point.x, point.y);
    updateBuffer(p);
  }
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
  wxImage img(width, height, (unsigned char *)Buffer, true);
  wxBitmap bmp(img);  
  dc.DrawBitmap(bmp, 0, 0, false);
  ///////////////////////////////////
}

bool Canvas::pasteFromClip(Transaction &txn) {
  if (wxTheClipboard->Open()) {
    if (wxTheClipboard->IsSupported(wxDF_BITMAP)) {
      /*
       * Copy bitmap / image:
       * (1) Read and cast clipboard bitmap as wxImage
       * (2) Get wxImage's internal data buffer which contains
       *     RGBRGBRGB.. data format of pixels, row major. 
       * (3) Check corresponding alpha values for each pixel.
       *     If alpha value of pixel is NOT 0 (i.e. not completely
       *     transparent), then copy that pixel's RGB.
       *     If alpha value of pixel is 0, then ignore pixel, as
       *     is transparent.
       * (4) Iterate through, add previous color to transactions,
       *     then update display buffer.
       * 
       * Note: Could try and optimize using memset, this
       * requires changes to the transaction system.
       */
      wxImage bmpImage;
      wxBitmapDataObject data;

      wxTheClipboard->GetData(data);
      bmpImage = data.GetBitmap().ConvertToImage();

      unsigned char *buffer;
      unsigned int N, M;
      N = bmpImage.GetHeight();
      M = bmpImage.GetWidth();
      buffer = bmpImage.GetData();

      int x, y;
      for (y=0; y<std::min(height, N); y++) {
        for (x=0; x<std::min(width, M); x++) {
          wxPoint p(x,y);
          Pixel pixel;
          Color prev_c = getPixelColor(p); /* prev color */
          Color c;
          int ind;

          ind = LOC(x, y, M);
          c = Color(
            buffer[ind],
            buffer[ind+1],
            buffer[ind+2]
          );

          pixel = Pixel(c, p);
          txn.update(Pixel(prev_c, p));

          updateBuffer(pixel);
        }
      }
    } 
    /* 
     * Add more options here:
     * e.g. wxDF_TEXT to process clipboard text input
     */

    // Always close the clipboard
    wxTheClipboard->Close();
  }
}

void Canvas::cpySelectToClip() {
  /* 
   * Note - Non-rectangular selection:
   * Use alpha channel to accept non-rectangular
   * selection areas.
   * e.g. Set alpha to 1 for transparent, so when
   * pasting, we can check alpha channel of 
   * the corresponding pixel to decide whether to
   * update it or not.
   */
  if (selected) {
    assert(selection != NULL);

    int N, M;
    N = selection->getHeight();
    M = selection->getWidth();

    Pixel pixel;
    int i;
    for (i=0; i<selectionArea.size(); i++) {
      pixel = selectionArea[i];

    }
  }
}

void Canvas::keyDownEvent(wxKeyEvent &evt) {
  char uc = evt.GetUnicodeKey();
  if (evt.ControlDown()) {
    switch (uc) {
      case (KEY_Z):
        if (!isUndo && transactions.size() > 0) {
          isUndo = true;

          Transaction *latest;
          latest = &transactions.back(); 
          revertTransaction(*latest);
          transactions.pop_back();
        }
        break;
      case (KEY_C):
        if (!isCopy) {
          printf("Copy!\n");
          isCopy = true;
          cpySelectToClip();           
        }
        break;
      case (KEY_V):
        if (!isPaste) {
          isPaste = true;
          Transaction txn;
          if (pasteFromClip(txn)) {
            currentTxn = txn;
            addTransaction(currentTxn);
          }
        }
        break;
      default: 
        break;
    }
  }
  wxWindow::Refresh();
}

void Canvas::keyUpEvent(wxKeyEvent & evt) {
  char uc = evt.GetUnicodeKey();
  // Z is released - not checking for Ctrl here on purpose
  switch(uc) {
    case (KEY_Z):
      isUndo = false;
      break;
    case (KEY_C):
      isCopy = false;
      break;
    case (KEY_V):
      isPaste = false;
      break;
    default:
      break; 
  }
}

/* Event handlers to handle CANVAS mouse events */
/*
 * Handle CLICK event
 * Drawing Tools: Pencil, Line, DrawRect, DrawCircle
 *    - Fill in the clicked pixel - all have the same functionality
 */
void Canvas::mouseDown(wxMouseEvent &evt)
{
  /* Always should be left is down */
  assert(evt.LeftIsDown());

  isNewTxn = true;
  int x = evt.GetX();
  int y = evt.GetY();
  prevPos = wxPoint(x, y);
  startPos = wxPoint(x, y);

  Transaction txn;
  Pixel p(color.r, color.g, color.b, x, y);
  switch(toolType) {
    case Eraser:
    case Pencil:
      freehand.clear();
      freehand.push_back(wxPoint(x, y));
    case Line:
    case DrawRect:
    case DrawCircle:
      break;
    case Fill:
      fill(startPos, color, txn);
      currentTxn = txn;
      break;
    case SlctRect:
      handleSelectionClick(startPos);
      break;
    case SlctCircle:
      handleSelectionClick(startPos);
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

  wxPoint currPos = wxPoint(evt.GetX(), evt.GetY());
  Transaction txn;
  switch(toolType) {
    case Pencil:
      freehand.push_back(currPos);
      updateBuffer(
        drawFreeHand(currPos, txn, 5),
        color);
      currentTxn = txn;
      break;
    case Line:
      updateBuffer(
        drawLine(currPos, txn),
        color);
      currentTxn = txn;
      break;
    case DrawRect:
      updateBuffer(
          drawRectangle(currPos, txn, 3),
          color);
      currentTxn = txn;
      break;
    case DrawCircle:
      updateBuffer(
        drawCircle(currPos, txn, 5),
        color);
      currentTxn = txn;
      break;
    case Eraser:
      freehand.push_back(currPos);
      updateBuffer(
          drawFreeHand(currPos, txn, 5),
          WHITE);
      currentTxn = txn;
      break;
    case SlctRect:
      handleSelectionMove(currPos, &Canvas::drawRectangle);
      break;
    case SlctCircle:
      handleSelectionMove(currPos, &Canvas::drawCircle);
      break;
    case Lasso:
      handleSelectionMove(currPos, &Canvas::drawFreeHand);
      break;
    default:
      break;
  }

  wxWindow::Refresh();
  prevPos = wxPoint(currPos.x, currPos.y);
  isNewTxn = false;
}

void Canvas::mouseReleased(wxMouseEvent &evt)
{
  wxPoint pt = wxPoint(evt.GetX(), evt.GetY());
  switch (toolType) {
    case SlctRect:
    case SlctCircle:
      handleSelectionRelease(startPos, pt);
      wxWindow::Refresh();
      break;
    default:
      break;
  }

  // commit transaction
  addTransaction(currentTxn);
}

std::vector<wxPoint>
Canvas::drawFreeHand(const wxPoint &currPos, Transaction &txn, const int &_width)
{
  if (!isNewTxn) {
    revertTransaction(currentTxn);
  }

  std::vector<wxPoint> points, _pts;
  wxPoint p0, p1;
  int i;
  for (i=0; i<freehand.size()-1; i++) {
    p0 = freehand[i];
    p1 = freehand[i+1];

    _pts = lerp(p0, p1, _width);
    points.insert(points.end(), _pts.begin(), _pts.end());
  }

  updateTransaction(txn, points);
  return points;
}

std::vector<wxPoint>
Canvas::drawCircle(const wxPoint &currPos, Transaction &txn, const int &_width) {
  std::vector<wxPoint> points;
  /*
   * Steps:
   * (1) If not first transaction, delete previous transaction.
   *     We must do this since we're constantly redrawing
   *     the circle's trace as the user is "dragging" across
   *     the screen 
   * (2) Interpolate some points such that the interpolated
   *     points form a circle by using distance b/w currPos
   *     and startPos
   * (3) Further interpolate by calling either linear or
   *     polynomial interpolation on the sparse points to
   *     generate the pixels
   * (4) Write all previous buffer values to the txn
   */  

  // (1)
  if (!isNewTxn) {
    revertTransaction(currentTxn);
  }

  /*
   * Steps for (2):
   * (2a) Find diameter/radius
   * (2b) Find center point
   * (2c) Traverse around the circle by using center point
   */
  double radius = length(currPos, startPos)/2;
  wxPoint c;
  {
    wxRealVec v; 
    wxVec _v = currPos - startPos;
    v = wxRealVec((double)_v.x, (double)_v.y);
    v = normalize(v);

    wxRealPoint _curr(startPos);
    c = wxPoint(_curr + radius*v);
  }

  /*
   * Number of samples depends on the circumference
   * of the circle
   */ 
  std::vector<wxPoint> draft;
  {
    wxRealPoint p;
    wxRealVec _u; 
    double _x, _y;
    double _P = (2*M_PI) / (2*M_PI*radius);
    double theta;
    for (theta=0.0; theta<2*M_PI; theta+=_P) {
      _x = cos(theta);
      _y = sin(theta);
      _u = normalize(wxRealVec(_x,_y)); // should be normal, but..

      p = wxRealPoint(c) + radius*_u;
      draft.push_back(wxPoint(p));
    }
  }

  // (3)
  {
    std::vector<wxPoint> pts;
    wxPoint p0, p1; 
    int i;
    for (i=0; i<draft.size()-1; i++) {
      p0 = draft[i];
      p1 = draft[i+1];

      pts = lerp(p0, p1, _width);
      points.insert(points.end(), pts.begin(), pts.end());
    } 
  }

  // (4)
  updateTransaction(txn, points);
  return points;
}

std::vector<wxPoint> 
Canvas::drawLine(const wxPoint &currPos, Transaction &txn) {
  std::vector<wxPoint> points; 
  /*
   * Steps:
   * (1) If isNewTxn, revert
   * (2) Interpolate line from startPos to currPos
   */
  if (!isNewTxn) {
    revertTransaction(currentTxn);
  } 

  points = lerp(startPos, currPos, 5);
  updateTransaction(txn, points);

  return points;
}

void
Canvas::fill(const wxPoint &p, const Color &color, Transaction &txn) {
  /*
   * Steps:
   * (1) From given point 'p', perform breadth-first
   *     algorithm, fill in every pixel whose color
   *     is the same as 'p'.
   * (2) Update given Transaction 'txn' with all filled
   *     pixels.
   * 
   * Note: we are not 'filling' in the pixel here, that is
   * done in calling event handler function. This function
   * simply passes back the points to be filled in. 
   */
  Color c = getPixelColor(p);
  if (c == color) {
    return;
  }

  int loc = LOC(p.x, p.y, width);
  txn.update(Pixel(c,p));
  Buffer[loc] = color.r;
  Buffer[loc+1] = color.g;
  Buffer[loc+2] = color.b;

  std::queue<wxPoint> Q;
  Q.push(p);

  wxPoint neighbors[4];
  int ncount;
  wxPoint _p;
  while (!Q.empty()) {
    _p = Q.front();
    Q.pop();

    getNeighbors(_p, neighbors, ncount);

    std::string _pstr;
    int _x, _y;
    int i;
    for (i=0; i<ncount; i++) {
      _x = neighbors[i].x;
      _y = neighbors[i].y;
      char r,g,b;
      loc = LOC(_x, _y, width);
      r = Buffer[loc];
      g = Buffer[loc+1];
      b = Buffer[loc+2];
      if (r == c.r && g == c.g && b == c.b) {
        txn.update(Pixel(r,g,b,_x,_y));
        Buffer[loc] = color.r;
        Buffer[loc+1] = color.g;
        Buffer[loc+2] = color.b;
        Q.push(wxPoint(_x,_y));
      }
    }
  }
}

std::vector<wxPoint>
Canvas::drawRectangle(const wxPoint &p1, Transaction &txn, const int &_width)
{
  /*
   * Approach:
   * 1. If isNewTxn is FALSE (i.e. user previously moved the mouse) then
   *    revert the current Transaction
   * 2. Get the pixels to fill in by performing linear interpolation on each
   *    pair of coordinates on the rectangle
   *      <x0, y0> -> <x1, y1>, <x0, y0> -> <x0, y1>
   *      <x1, y0> -> <x1, y1>, <x1, y1> -> <x1, y0>
   * 3. Update currentTxn to include all points from 2.
   * 4. Return the points
   */
  // (1)
  if (!isNewTxn) {
    revertTransaction(currentTxn);
  }

  // (2)
  std::vector<wxPoint> points;

  {
    wxPoint p2 = wxPoint(startPos.x, p1.y);
    wxPoint p3 = wxPoint(p1.x, startPos.y);
    std::vector<wxPoint> lines[4];

    lines[0] = lerp(startPos, p3, _width);
    lines[1] = lerp(p1, p3, _width);
    lines[2] = lerp(p1, p2, _width);
    lines[3] = lerp(startPos, p2, _width);

    int i;
    for (i=0; i < 4; i++) {
      points.insert(points.end(), lines[i].begin(), lines[i].end());
    }
  }

  updateTransaction(txn, points);
  return points;
}

/*
 * Used for generating Selection Border
 * - Given a border, remove points on the border
 */
std::vector<wxPoint>
Canvas::makeDashed(const std::vector<wxPoint> &border)
{
  std::vector<wxPoint> dashed;
  int i;
  for (i=0; i < border.size()-5; i++) {
    if (i % 8 == 0) {
      if (i+5 >= border.size()) {
        break;
      }
      dashed.insert(
          dashed.end(),
          border.begin() + i,
          border.begin() + i + 5);
    }
  }
  return dashed;
}

void printPixels(std::vector<Pixel> pixels) {
  int i;
  Pixel p;
  for (i=0; i < pixels.size(); i++) {
    p = pixels[i];
    printf("pixel: (%d, %d) (%d %d %d)\n",
        p.x,
        p.y,
        p.color.r,
        p.color.g,
        p.color.b);
  }
}

/*
 * Reset all selection related fields
 */
void Canvas::clearSelection() {
  selected = false;
  selectionArea.clear();
  selectionBorder.clear();
  revertTransaction(selectTxn);
  selectTxn.pixels.clear();
  delete selection;
  selection = NULL;
}

void
Canvas::handleSelectionClick(wxPoint &pt)
{
  /*
   * Two cases to consider:
   * 1. User doesn't have any current selections
   *    - Same as DrawRect
   * 2. User has a selection
   *    a) If user clicks ON the selection - they can move it
   *    b) NOT on the selection - erase the selection border)
   *    -> now the user no longer has a selection
   */

  // Case 1)
  if (!selected) {
    Pixel p = Pixel(SELECT, pt);
    updateBuffer(p);
    currentTxn.update(p);
  }
  // Case 2)
  else {
    if (!selection->isWithinBounds(pt)) {
      clearSelection();
    }
  }

}

/*
 * Functions to set the selection area
 * based on the Selection object.
 * One function for each type selection.
 */
void
Canvas::getSelectionArea(
    std::vector<Pixel> &area,
    RectangleSelection *selection)
{
  /*
   * The width and height of the selection
   * should include the pixels occupied
   * by the selection border
   */
  int _width = (selection->maxX - selection->minX) + 1;
  int _height = (selection->maxY - selection->minY) + 1;
  area.resize(_width * _height);

  int startX = selection->minX;
  int startY = selection->minY;
  int endX = startX + _width;
  int endY = startY + _height;

  /* Set the selectionArea pixels */
  int i, j, k=0;
  Pixel p;
  wxPoint pt;
  for (i=startX; i < endX; i++) {
    for (j=startY; j < endY; j++) {
      pt = wxPoint(i, j);
      p = getPixel(pt);
      area[k] = p;
      k++;
    }
  }
}

void
Canvas::getSelectionArea(
    std::vector<Pixel> &selectionArea,
    CircleSelection *selection)
{
  wxPoint c = selection->_c;
  double r = selection->_r;

  int minX, maxX, minY, maxY;
  minX = selection->minX;
  maxX = selection->maxX;
  minY = selection->minY;
  maxY = selection->maxY;

  int x, y;
  wxPoint pt;
  for (x=minX; x < maxX+1; x++) {
    for (y=minY; y < maxY+1; y++) {
      pt = wxPoint(x, y);
      if (squaredLength(pt, c) <= r*r) {
        selectionArea.push_back(
            getPixel(pt));
      }
    }
  }

}

void
Canvas::getSelectionArea(
    std::vector<Pixel> &selectionArea,
    LassoSelection *selection)
{
  // TODO
}

/*
 * Handles mouseMove event for selection tools
 * Takes in a "drawBorder" parameter which points to one
 * of: drawRectangle(), drawCircle(), or drawFreehand()
 * depending on the tool type.
 */
void
Canvas::handleSelectionMove(const wxPoint &currPos,
  std::vector<wxPoint> (Canvas::*drawBorder)(const wxPoint&, Transaction &, const int&))
{
  /*
   * Two cases to consider:
   * 1. User hasn't made a selection
   *    - Have to draw the selection border
   * 2. User has a selection
   *    - Have to move the selected pixels to the
   *      new position
   */
  if (!selected) {
    Transaction txn, sTxn;
    if (!isNewTxn)
      revertTransaction(currentTxn);

    selectionBorder = (this->*drawBorder)(currPos, txn, 1);
    updateBuffer(
        makeDashed(selectionBorder),
        SELECT);

    selectTxn = txn;
    currentTxn = txn;
  }
  else {
    int xOffset = currPos.x - startPos.x;
    int yOffset = currPos.y - startPos.y;
    Transaction txn;
    move(selectionArea, xOffset, yOffset, txn);
    currentTxn = txn;
  }
}

void
Canvas::handleSelectionRelease(const wxPoint &p0, const wxPoint &p1)
{
  /*
   * Two cases to consider:
   * 1. User has not made a selection yet
   *      The release action will complete the selection
   *      process and define the selectionArea.
   * 2. User has a selection already
   *      Clear the selection.
   */
  if (selected) {
    int xOffset = p1.x - p0.x;
    int yOffset = p1.y - p0.y;

    int i, x, y;
    for (i=0; i < selectionBorder.size(); i++) {
      selectionBorder[i].x += xOffset;
      selectionBorder[i].y += yOffset;
    }
    clearSelection();
  }
  else {
    switch (toolType) {
      case SlctRect:
        selection = new RectangleSelection(p0, p1);
        getSelectionArea(selectionArea,
            dynamic_cast<RectangleSelection *>(selection));
        break;
      case SlctCircle:
        selection = new CircleSelection(p0, p1);
        getSelectionArea(selectionArea,
            dynamic_cast<CircleSelection *>(selection));
        break;
      case Lasso:
        break;
      default:
        break;
    }
    selected = true;
  }
}

void
Canvas::move(
    const std::vector<Pixel> &pixels,
    const int &xOffset, const int &yOffset,
    Transaction &txn)
{
  /*
   * Step:
   * 1. White out the /original/ selection
   * 2. Save the border pixels
   * 3. Translate the selection
   */
  if (!isNewTxn) {
    revertTransaction(currentTxn);
  }
  // (1) white out the selection
  {
    int i;
    wxPoint oldPt;
    Pixel pixel, whitePixel;
    for (i=0; i < selectionArea.size(); i++) {
      pixel = selectionArea[i];

      oldPt = wxPoint(pixel.x, pixel.y);
      txn.update(pixel);
      whitePixel = Pixel(Color(255, 255, 255), oldPt);

      updateBuffer(whitePixel);
    }
  }

  // (2) Save border pixels
  {
    int i;
    wxPoint pt, newPt;
    selectTxn.pixels.resize(selectionBorder.size());
    assert(selectionBorder.size() == selectTxn.pixels.size());
    for (i=0; i < selectionBorder.size(); i++) {
      pt = selectionBorder[i];
      newPt = wxPoint(pt.x + xOffset, pt.y + yOffset);
      selectTxn.pixels[i] = Pixel(getPixelColor(newPt), newPt);
    }
  }

  // (3) translate the selection
  {
    int i;
    wxPoint newPt;
    Color clr;
    Pixel pixel, oldPixel, newPixel, p;
    for (i=0; i < selectionArea.size(); i++) {
      pixel = selectionArea[i];
      clr = pixel.color;

      newPt = wxPoint(
          pixel.x + xOffset,
          pixel.y + yOffset);
      oldPixel = Pixel(getPixelColor(newPt), newPt);
      txn.update(oldPixel);
      newPixel = Pixel(clr, newPt);

      updateBuffer(newPixel);
    }
  }
}