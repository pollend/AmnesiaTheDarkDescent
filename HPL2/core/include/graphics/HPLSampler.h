#pragma once

class HPLSampler {
public:
  HPLSampler();
  ~HPLSampler();

//  //returns the same sampler if one hasn't been returned.
//  Sampler* fetch(Renderer* renderer);

  //free the internal sample will be re-initialized when fetched the next time.
  void free();
//
//private:
//  Sampler* _sampler;
};