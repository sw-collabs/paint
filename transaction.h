#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <vector>
#include "pixel.h"

class Transaction {
  private:

  public:
    std::vector<Pixel> pixels;

    inline Transaction();
    inline void update(Pixel &p); 
    inline void update(Pixel &&p);
    inline void insert(Transaction &txn);
};

inline Transaction::Transaction() {}

inline void Transaction::update(Pixel &p) {
  pixels.push_back(p);
}

inline void Transaction::update(Pixel &&p) {
  pixels.push_back(p);
}

inline void Transaction::insert(Transaction &txn) {
  pixels.insert(pixels.end(), 
    txn.pixels.begin(), txn.pixels.end());
}

#endif /* TRANSACTION_H */
