#include "DetectionCooldown.h"
#include <cassert>
using namespace callingcard;
int main() {
 DetectionCooldown c;
 assert(!c.accept(DetectionType::Unknown,"test",'W',0));
 assert(!c.accept(DetectionType::Flock,"",'W',0));
 assert(c.accept(DetectionType::Flock,"flock-ieee-b41e52",'W',0));
 assert(!c.accept(DetectionType::Flock,"flock-ieee-b41e52",'W',10));
 assert(!c.accept(DetectionType::Flock,"flock-ieee-b41e52",'W',29999));
 assert(c.accept(DetectionType::Flock,"flock-ieee-b41e52",'W',30000));
}
