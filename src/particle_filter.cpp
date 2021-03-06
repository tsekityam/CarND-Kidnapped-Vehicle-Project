/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of
	//   x, y, theta and their uncertainties from GPS) and all weights to 1.
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

	num_particles = 1000;
	for (size_t i = 0; i < num_particles; i++) {
		Particle particle;
		particle.id = i;

		random_device rd;
		default_random_engine gen(rd());
		normal_distribution<> d_x(x,std[0]);
		normal_distribution<> d_y(y,std[1]);
		normal_distribution<> d_theta(theta,std[2]);

		particle.x = d_x(gen);
		particle.y = d_y(gen);
		particle.theta = d_theta(gen);
		particle.weight = 1;

		weights.push_back(particle.weight);
		particles.push_back(particle);
	}

	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	for (size_t i = 0; i < particles.size(); i++) {
		double new_theta = particles.at(i).theta + yaw_rate*delta_t;
		if (fabs(yaw_rate) < 0.0001) {
			particles.at(i).x += velocity*delta_t*cos(particles.at(i).theta);
			particles.at(i).y += velocity*delta_t*sin(particles.at(i).theta);
		}
		else {
			particles.at(i).x += (velocity/yaw_rate)*(sin(new_theta)-sin(particles.at(i).theta));
			particles.at(i).y += (velocity/yaw_rate)*(cos(particles.at(i).theta)-cos(new_theta));;
			particles.at(i).theta = new_theta;
		}

		random_device rd;
		default_random_engine gen(rd());
		normal_distribution<> d_x(particles.at(i).x,std_pos[0]);
		normal_distribution<> d_y(particles.at(i).y,std_pos[1]);
		normal_distribution<> d_theta(particles.at(i).theta,std_pos[2]);

		particles.at(i).x = d_x(gen);
		particles.at(i).y = d_y(gen);
		particles.at(i).theta = d_theta(gen);
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to
	//   implement this method and use it as a helper during the updateWeights phase.

	for (size_t i = 0; i < observations.size(); i++) {
		double min_dist = INFINITY;
		for (size_t j = 0; j < predicted.size(); j++) {
			double current_dist = dist(predicted.at(j).x, predicted.at(j).y, observations.at(i).x, observations.at(i).y);
			if (current_dist < min_dist) {
				min_dist = current_dist;
				observations.at(i).id = predicted.at(j).id;
			}
		}
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		std::vector<LandmarkObs> observations, Map map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	for (size_t i = 0; i < particles.size(); i++) {
		std::vector<LandmarkObs> predicted;
		std::vector<LandmarkObs> transformed_observations;

		for (auto obs: observations) {
			LandmarkObs transformed_obs;
			transformed_obs.x = obs.x*cos(particles.at(i).theta)-obs.y*sin(particles.at(i).theta)+particles.at(i).x;
			transformed_obs.y = obs.x*sin(particles.at(i).theta)+obs.y*cos(particles.at(i).theta)+particles.at(i).y;
			transformed_observations.push_back(transformed_obs);
		}

		for (auto landmark: map_landmarks.landmark_list) {
			if (dist(landmark.x_f, landmark.y_f, particles.at(i).x, particles.at(i).y) < sensor_range) {
				LandmarkObs obs;
				obs.id = landmark.id_i;
				obs.x = landmark.x_f;
				obs.y = landmark.y_f;
				predicted.push_back(obs);
			}
		}

		dataAssociation(predicted, transformed_observations);
		particles.at(i).weight = 1;

		for (auto transformed_obs: transformed_observations) {
			for (auto landmark: predicted) {
				if (landmark.id == transformed_obs.id) {
					double delta_x = landmark.x - transformed_obs.x;
					double delta_y = landmark.y - transformed_obs.y;
					double exp_value = exp(-(pow(delta_x, 2)/(2*pow(std_landmark[0], 2))+pow(delta_y, 2)/(2*pow(std_landmark[1], 2))));
					particles.at(i).weight *= 1/(2*M_PI*std_landmark[0]*std_landmark[1])*exp_value;
					break;
				}
			}
		}

		weights.at(i) = particles.at(i).weight;
	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight.
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
	random_device rd;
	default_random_engine gen(rd());
	discrete_distribution<> d(weights.begin(), weights.end());

	vector<Particle> new_particles;
	for (size_t i = 0; i < num_particles; i++) {
		new_particles.push_back(particles.at(d(gen)));
	}
	particles = new_particles;
}

Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)
{
	//particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates

	//Clear the previous associations
	particle.associations.clear();
	particle.sense_x.clear();
	particle.sense_y.clear();

	particle.associations= associations;
 	particle.sense_x = sense_x;
 	particle.sense_y = sense_y;

 	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
