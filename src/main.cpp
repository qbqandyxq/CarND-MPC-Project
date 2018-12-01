#include "json.hpp"
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "MPC.h"

// for convenience
using json = nlohmann::json;
using namespace std;
// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
std::string hasData(std::string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.rfind("}]");
    if (found_null != std::string::npos) {
    return "";
  } else if (b1 != std::string::npos && b2 != std::string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

// Evaluate a polynomial.
double polyeval(Eigen::VectorXd coeffs, double x) {
  double result = 0.0;
  for (int i = 0; i < coeffs.size(); i++) {
    result += coeffs[i] * pow(x, i);
  }
  return result;
}

// Fit a polynomial.
// Adapted from
// https://github.com/JuliaMath/Polynomials.jl/blob/master/src/Polynomials.jl#L676-L716
Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals,
                        int order) {
  assert(xvals.size() == yvals.size());
  assert(order >= 1 && order <= xvals.size() - 1);
  Eigen::MatrixXd A(xvals.size(), order + 1);

  for (int i = 0; i < xvals.size(); i++) {
    A(i, 0) = 1.0;
  }

  for (int j = 0; j < xvals.size(); j++) {
    for (int i = 0; i < order; i++) {
      A(j, i + 1) = A(j, i) * xvals(j);
    }
  }

  auto Q = A.householderQr();
  auto result = Q.solve(yvals);
  return result;
}
/*
 ptsx (Array) - The global x positions of the waypoints.
 ptsy (Array) - The global y positions of the waypoints. This corresponds to the z coordinate in Unity since y is the up-down direction.
 psi (float) - The orientation of the vehicle in radians converted from the Unity format to the standard format expected in most mathemetical functions (more details below).
 psi_unity (float) - The orientation of the vehicle in radians. This is an orientation commonly used in navigation.
 x (float) - The global x position of the vehicle.
 y (float) - The global y position of the vehicle.
 steering_angle (float) - The current steering angle in radians.
 throttle (float) - The current throttle value [-1, 1].
 speed (float) - The current velocity in mph.
 */

int main() {
  uWS::Hub h;
  // MPC is initialized here!
  MPC mpc;
  h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
      std::string sdata = std::string(data).substr(0, length);
      std::cout << sdata << std::endl;
    if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
        std::string s = hasData(sdata);
      if (s != "") {
        auto j = json::parse(s);
        std::string event = j[0].get<std::string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          vector<double> ptsx = j[1]["ptsx"];
          vector<double> ptsy = j[1]["ptsy"];
          double px = j[1]["x"];
          double py = j[1]["y"];
          double psi = j[1]["psi"];
          double v = j[1]["speed"];
            double delta= j[1]["steering_angle"];
            double a = j[1]["throttle"];
          /*
          * TODO: Calculate steering angle and throttle using MPC.
          *
          * Both are in between [-1, 1].
          *
          */
          double steer_value=0.0;
          double throttle_value=0.0;
//            100ms latency.
            double latency = 0.1;
            
            psi += -v*delta/Lf*latency/2.67;
            v += a*latency;
            
        size_t n_waypoints = ptsx.size();
        auto ptsx_transformed = Eigen::VectorXd(n_waypoints);
        auto ptsy_transformed = Eigen::VectorXd(n_waypoints);
            
        for (unsigned int i = 0; i < n_waypoints; i++ ) {
            double dX = ptsx[i] - px;
            double dY = ptsy[i] - py;
            double minus_psi = 0.0 - psi;
            ptsx_transformed( i ) = dX * cos( minus_psi ) - dY * sin( minus_psi );
            ptsy_transformed( i ) = dX * sin( minus_psi ) + dY * cos( minus_psi );
        }
            auto coeffs = polyfit(ptsy_transformed, ptsy_transformed,3);
            // State after delay.
            
//            double v += a * latency;
//            double cte_delay = coeffs[0] + ( v * sin(-atan(coeffs[1])) * latency );
//            double epsi_delay = -atan(coeffs[1]) - ( v * atan(coeffs[1]) * latency / mpc.Lf );
            double cte = polyeval(coeffs, 0);
            double epsi = -atan(coeffs[1]);
            
            Eigen::VectorXd state(6);
            state << 0, 0, 0, v, cte, epsi;
            
	    Eigen::VectorXd state(6);
            state <<0, 0, 0, v,cte, epsi; //cte_delay, epsi_delay;
            //get the solution
            auto vars = mpc.Solve(state, coeffs);
	    
            steer_value=vars[0]/deg2rad(25);
            throttle_value = vars[1];
            
          json msgJson;
          // NOTE: Remember to divide by deg2rad(25) before you send the steering value back.
          // Otherwise the values will be in between [-deg2rad(25), deg2rad(25] instead of [-1, 1].
          msgJson["steering_angle"] = steer_value;
          msgJson["throttle"] = throttle_value;

          //Display the MPC predicted trajectory 
          vector<double> mpc_x_vals;
          vector<double> mpc_y_vals;
            for(int i=2; i<vars.size(); i=i+2){
                mpc_x_vals.push_back(vars[i]);
                mpc_y_vals.push_back(vars[i+1]);
            }
          //.. add (x,y) points to list here, points are in reference to the vehicle's coordinate system
          // the points in the simulator are connected by a Green line

          msgJson["mpc_x"] = mpc_x_vals;
          msgJson["mpc_y"] = mpc_y_vals;

          //Display the waypoints/reference line
          vector<double> next_x_vals;
          vector<double> next_y_vals;
          for (unsigned int i = 0 ; i < ptsx.size(); ++i) {
              next_x_vals.push_back(ptsx_transformed[i]);
              next_y_vals.push_back(ptsy_transformed[i]);
              
          }
//            double poly_inc = 2.5;
//            int num_points = 10;
//            for ( int i = 0; i < num_points; i++ ) {
//                double x = poly_inc * i;
//                next_x_vals.push_back( x );
//                next_y_vals.push_back(polyeval(coeffs, x) );
//            }
		
          //.. add (x,y) points to list here, points are in reference to the vehicle's coordinate system
          // the points in the simulator are connected by a Yellow line
            
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;


          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          std::cout << msg << std::endl;
          // Latency
          // The purpose is to mimic real driving conditions where
          // the car does actuate the commands instantly.
          //
          // Feel free to play around with this value but should be to drive
          // around the track with 100ms latency.
          //
          // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE
          // SUBMITTING.
          this_thread::sleep_for(chrono::milliseconds(100));
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
