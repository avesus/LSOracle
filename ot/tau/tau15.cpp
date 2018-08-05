#include <ot/tau/tau15.hpp>
#include <ot/timer/timer.hpp>

namespace ot::tau15 {

// Procedure: read
void Timing::read(const std::filesystem::path& path) {
  
  std::string line, token, pin;
  
  // ------------------------------------------------------
  // Read timing file.
  // ------------------------------------------------------
  std::ifstream tfs(path);

  if(!tfs.good()) {
    OT_LOGE("can't open timing assertions ", path);
    return;
  }
  
  while(std::getline(tfs, line)) {
    
    std::istringstream ss(line);

    ss >> token;

    if(token == "clock") {
      Clock clock;
      ss >> clock.pin >> clock.period;
      assertions.push_back(std::move(clock));
    }
    else if(token == "at") {
      AT at;
      ss >> at.pin 
         >> at.value[ot::EARLY][ot::RISE] >> at.value[ot::EARLY][ot::FALL] 
         >> at.value[ot::LATE][ot::RISE]  >> at.value[ot::LATE][ot::FALL];
      assertions.push_back(std::move(at));
    }
    else if(token == "slew") {
      Slew slew;
      ss >> slew.pin
         >> slew.value[ot::EARLY][ot::RISE] >> slew.value[ot::EARLY][ot::FALL]
         >> slew.value[ot::LATE][ot::RISE] >> slew.value[ot::LATE][ot::FALL];
      assertions.push_back(std::move(slew));
    }
    else if(token == "rat") {
      RAT rat;
      ss >> rat.pin
         >> rat.value[ot::EARLY][ot::RISE] >> rat.value[ot::EARLY][ot::FALL]
         >> rat.value[ot::LATE][ot::RISE] >> rat.value[ot::LATE][ot::FALL];
      assertions.push_back(std::move(rat));
    }
    else if(token == "load") {
      Load load;
      ss >> load.pin >> load.value;
      assertions.push_back(std::move(load));
    }
    else {
      OT_LOGF("unknown keyword ", token);
    }
  }
}

};  // end of namespace ot::tau15. ----------------------------------------------------------------

namespace ot {

// Function: timing
Timer& Timer::timing(std::filesystem::path path) {

  auto timing = std::make_shared<tau15::Timing>();

  std::scoped_lock lock(_mutex);
  
  // Library reader
  auto reader = _taskflow.silent_emplace([this, path=std::move(path), timing] () {
    OT_LOGI("loading timing ", path, " ...");
    timing->read(path);
  });

  // Placeholder to add_lineage
  auto modifier = _taskflow.silent_emplace([this, timing] () {
    OT_LOGI("add ", timing->assertions.size(), " timing assertions");
    _timing(*timing);
  });

  // Reader -> modifier
  reader.precede(modifier);

  // parent -> modifier
  _add_to_lineage(modifier);
  
  return *this;
}

// Procedure: _timing
void Timer::_timing(tau15::Timing& timing) {

  for(auto& ast : timing.assertions) {
    std::visit(Functors{
      [&] (tau15::Clock& clock) {
        if(auto itr = _pins.find(clock.pin); itr != _pins.end()) {
          _clock(itr->first, itr->second, clock.period);
        }
        else {
          OT_LOGE("can't create clock (pin ", clock.pin, " not found)");
        }
      },
      [&] (tau15::AT& a) {
        if(auto itr = _pis.find(a.pin); itr != _pis.end()) {
          FOR_EACH_EL_RF(el, rf) {
            _at(itr->second, el, rf, a.value[el][rf]);
          }
        }
        else {
          OT_LOGE("can't set arrival time (PI ", a.pin, " not found)");
        }
      },
      [&] (tau15::Slew& s) {
        if(auto itr = _pis.find(s.pin); itr != _pis.end()) {
          FOR_EACH_EL_RF(el, rf) {
            _slew(itr->second, el, rf, s.value[el][rf]);
          }
        }
        else {
          OT_LOGE("can't set slew (PI ", s.pin, " not found)");
        }
      },
      [&] (tau15::RAT& r) {
        if(auto itr = _pos.find(r.pin); itr != _pos.end()) {
          FOR_EACH_EL_RF(el, rf) {
            _rat(itr->second, el, rf, r.value[el][rf]);
          }
        }
        else {
          OT_LOGE("can't set rat (PO ", r.pin, " not found)");
        }
      },
      [&] (tau15::Load& l) {
        if(auto itr = _pos.find(l.pin); itr != _pos.end()) {
          FOR_EACH_EL_RF(el, rf) {
            _load(itr->second, el, rf, l.value);
          }
        }
        else {
          OT_LOGE("can't set load (PO ", l.pin, " not found)");
        }
      }
    }, ast);
  }
}

};  // end of namespace ot ------------------------------------------------------------------------
