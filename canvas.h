//
// Created by Patricia M Marukot on 2020-04-30.
//

#ifndef PAINT_CANVAS_H
#define PAINT_CANVAS_H

#include "transaction.h"
#include "pixel.h"

enum ToolType
{
  Pencil,
  Line,
  DrawRect,
  DrawCircle,
  Eraser,
  Fill,
  SlctRect,
  SlctCircle,
  Lasso
};

class Canvas : public wxPanel {
  private:
    /* In pixels */
    unsigned int width;
    unsigned int height;

    /* The colour selected by the user */
    Color color;

    /* The previous mouse position
     * before mouse move event */
    wxPoint prevPos;

    /* The mouse position where the user first
     * clicked the left mouse button*/
    wxPoint startPos;

    /* Holds the transaction currently
     * taking place.
     * There should only be ONE txn occuring
     * at a time */
    bool isNewTxn;
    Transaction currentTxn;
    Transaction selectTxn;

    /* TRUE if user has made a selection */
    bool selected;
    std::vector<Pixel> selectionArea;
    std::vector<wxPoint> selectionBorder;

    /* Sampled points for freehand */
    std::vector<wxPoint> freehand;

    /* This is the main buffer that is drawn to the screen */
    char *Buffer;
    std::vector<Transaction> transactions;
    
    /* For Ctrl+Z - redo options */
    bool isRedo = false;
    bool isUndo = false;

    /*
     * Private functions
     */
    Pixel getPixel(const wxPoint &p);
    Color getPixelColor(const wxPoint &p);
    void getNeighbors(const wxPoint &p, wxPoint *neighbors, int &ncount);

    void updateBuffer(const std::vector<wxPoint> &points, const Color &color);
    void updateBuffer(const Pixel &p);
    void addTransaction(Transaction &txn);
    void revertTransaction(Transaction &txn);
    void updateTransaction(Transaction &txn, const std::vector<wxPoint> &points);

    std::vector<wxPoint> drawFreeHand(Transaction &txn);
    std::vector<wxPoint> drawRectangle(const wxPoint &p0, const wxPoint &p1, Transaction &txn);
    std::vector<wxPoint> drawCircle(const wxPoint &currPos, Transaction &txn);
    std::vector<wxPoint> drawLine(const wxPoint &currPos, Transaction &txn);
    void fill(const wxPoint &p, const Color &color, Transaction &txn);

    /* Event handlers for rectangle selection */
    void handleSelectRectClick(wxPoint &pt);
    void handleSelectRectMove(const wxPoint &p0, const wxPoint &p1);
    void handleSelectRectRelease(const wxPoint &p0, const wxPoint &p1);

    void move(const std::vector<Pixel> &pixels,
        const int &xOffset, const int &yOffset, Transaction &txn);

  public:
    Canvas(wxFrame *parent);
    Canvas(wxFrame *parent, unsigned int width, unsigned int height);
    ToolType toolType;

    /* Screen refresh event handlers */
    void paintEvent(wxPaintEvent & evt);
    void paintNow();

    /* Mouse event handlers */
    void keyDownEvent(wxKeyEvent & evt);
    void keyUpEvent(wxKeyEvent & evt);
    void mouseDown(wxMouseEvent & evt);
    void mouseMoved(wxMouseEvent & evt);
    void mouseReleased(wxMouseEvent & evt);

    void render(wxDC& dc);
    DECLARE_EVENT_TABLE()
};

#endif //PAINT_CANVAS_H
