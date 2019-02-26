#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <sstream>
#include <string>
#include <unordered_map>

using namespace std;

class LRUCache {
 public:
  LRUCache(int capacity) {
    _capacity = capacity;
    _miss = _access = 0;
    _dll = list<int>();
    _map = unordered_map<int, list<int>::iterator>();
  }

  void access(int key) {
    _access++;
    // find in map
    if (_map.find(key) == _map.end()) {
      _miss++;
      if (_dll.size() == _capacity) {
        // remove LRU item
        int temp = _dll.back();
        _dll.pop_back();
        _map.erase(temp);
      }
    } else {
      // delete from list
      _dll.erase(_map[key]);
    }

    // move key to front
    _dll.push_front(key);
    _map[key] = _dll.begin();
  }

  int getMiss() { return _miss; }

  int getTotalAccess() { return _access; }

 private:
  list<int> _dll;
  unordered_map<int, list<int>::iterator> _map;
  int _capacity, _size, _miss, _access;
};

void plotting(string filename) {
  int min, max, inc;
  printf("\nEnter [min] [max] [increment] for cache size to plot: \n");
  cin >> min;
  cin >> max;
  cin >> inc;

  for (int i = min; i <= max; i = i + inc) {
    fstream file;
    file.open(filename, ios::in);

    if (!file.is_open()) {
      cerr << "Failed to open file!" << endl;
      exit(0);
    }

    LRUCache cache(i);

    string line;
    while (getline(file, line)) {
      istringstream iss(line);
      int key;
      if (!(iss >> key)) {
        continue;
      }
      cache.access(key);
    }

    int miss = cache.getMiss();
    int total = cache.getTotalAccess();

    double mrate = (double)miss / total;

    cout << i << " " << mrate << endl;
    // delete &cache;
    file.close();
  }

  exit(0);
}

int main(int argc, char** argv) {
  if (argc < 3) {
    printf("Input Ex: %s <cache Size> <tracefile>\n", argv[0]);
    exit(0);
  }

  int size = 0;  // cache size
  sscanf(argv[1], "%d", &size);

  string filename = argv[2];

  if (size == -1)
    plotting(filename);

  fstream file;
  file.open(filename, ios::in);

  if (!file.is_open()) {
    cerr << "Failed to open file!" << endl;
    exit(0);
  }

  LRUCache cache(size);
  /*
      int key
      while(file >> key) {
          cache.access(key);
      }
      file.close();
  */
  string line;
  while (getline(file, line)) {
    istringstream iss(line);
    int key;
    if (!(iss >> key)) {
      continue;
    }
    // cout << key << endl;
    cache.access(key);
  }

  int miss = cache.getMiss();
  int total = cache.getTotalAccess();

  cout << "Access: " << total << " Hits: " << total - miss << " Miss: " << miss
       << endl;

  return 0;
}
