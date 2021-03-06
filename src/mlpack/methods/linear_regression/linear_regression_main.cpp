/**
 * @file linear_regression_main.cpp
 * @author James Cline
 *
 * Main function for least-squares linear regression.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#include <mlpack/prereqs.hpp>
#include <mlpack/core/util/cli.hpp>
#include <mlpack/core/util/mlpack_main.hpp>

#include "linear_regression.hpp"

using namespace mlpack;
using namespace mlpack::regression;
using namespace mlpack::util;
using namespace arma;
using namespace std;

PROGRAM_INFO("Simple Linear Regression and Prediction",
    "An implementation of simple linear regression and simple ridge regression "
    "using ordinary least squares. This solves the problem"
    "\n\n"
    "  y = X * b + e"
    "\n\n"
    "where X (specified by " + PRINT_PARAM_STRING("training") + ") and y "
    "(specified either as the last column of the input matrix " +
    PRINT_PARAM_STRING("training") + " or via the " +
    PRINT_PARAM_STRING("training_responses") + " parameter) are known and b is"
    " the desired variable.  If the covariance matrix (X'X) is not invertible, "
    "or if the solution is overdetermined, then specify a Tikhonov "
    "regularization constant (with " + PRINT_PARAM_STRING("lambda") + ") "
    "greater than 0, which will regularize the covariance matrix to make it "
    "invertible.  The calculated b may be saved with the " +
    PRINT_PARAM_STRING("output_predictions") + " output parameter."
    "\n\n"
    "Optionally, the calculated value of b is used to predict the responses for"
    " another matrix X' (specified by the " + PRINT_PARAM_STRING("test") + " "
    "parameter):"
    "\n\n"
    "   y' = X' * b"
    "\n\n"
    "and the predicted responses y' may be saved with the " +
    PRINT_PARAM_STRING("output_predictions") + " output parameter.  This type "
    "of regression is related to least-angle regression, which mlpack "
    "implements as the 'lars' program."
    "\n\n"
    "For example, to run a linear regression on the dataset " +
    PRINT_DATASET("X") + " with responses " + PRINT_DATASET("y") + ", saving "
    "the trained model to " + PRINT_MODEL("lr_model") + ", the following "
    "command could be used:"
    "\n\n" +
    PRINT_CALL("linear_regression", "training", "X", "training_responses", "y",
        "output_model", "lr_model") +
    "\n\n"
    "Then, to use " + PRINT_MODEL("lr_model") + " to predict responses for a "
    "test set " + PRINT_DATASET("X_test") + ", saving the predictions to " +
    PRINT_DATASET("X_test_responses") + ", the following command could be "
    "used:"
    "\n\n" +
    PRINT_CALL("linear_regression", "input_model", "lr_model", "test", "X_test",
        "output_predictions", "X_test_responses"));

PARAM_MATRIX_IN("training", "Matrix containing training set X (regressors).",
    "t");
PARAM_ROW_IN("training_responses", "Optional vector containing y "
    "(responses). If not given, the responses are assumed to be the last row "
    "of the input file.", "r");

PARAM_MODEL_IN(LinearRegression, "input_model", "Existing LinearRegression "
    "model to use.", "m");
PARAM_MODEL_OUT(LinearRegression, "output_model", "Output LinearRegression "
    "model.", "M");

PARAM_MATRIX_IN("test", "Matrix containing X' (test regressors).", "T");

// This is the future name of the parameter.
PARAM_COL_OUT("output_predictions", "If --test_file is specified, this "
    "matrix is where the predicted responses will be saved.", "o");

PARAM_DOUBLE_IN("lambda", "Tikhonov regularization for ridge regression.  If 0,"
    " the method reduces to linear regression.", "l", 0.0);

static void mlpackMain()
{
  const double lambda = CLI::GetParam<double>("lambda");

  RequireOnlyOnePassed({ "training", "input_model" }, true);

  ReportIgnoredParam({{ "test", true }}, "output_predictions");

  mat regressors;
  rowvec responses;

  LinearRegression lr;
  lr.Lambda() = lambda;

  bool computeModel = false;
  if (!CLI::HasParam("input_model"))
    computeModel = true;

  // If they specified a model file, we also need a test file or we
  // have nothing to do.
  if (!computeModel)
  {
    RequireAtLeastOnePassed({ "test" }, true, "test points must be specified "
        "when an input model is given");
  }

  ReportIgnoredParam({{ "input_model", true }}, "lambda");

  RequireAtLeastOnePassed({ "output_model", "output_predictions" }, false,
      "no output will be saved");

  // An input file was given and we need to generate the model.
  if (computeModel)
  {
    Timer::Start("load_regressors");
    regressors = std::move(CLI::GetParam<mat>("training"));
    Timer::Stop("load_regressors");

    // Are the responses in a separate file?
    if (!CLI::HasParam("training_responses"))
    {
      // The initial predictors for y, Nx1.
      responses = regressors.row(regressors.n_rows - 1);
      regressors.shed_row(regressors.n_rows - 1);
    }
    else
    {
      // The initial predictors for y, Nx1.
      Timer::Start("load_responses");
      responses = CLI::GetParam<rowvec>("training_responses");
      Timer::Stop("load_responses");

      if (responses.n_cols != regressors.n_cols)
      {
        Log::Fatal << "The responses must have the same number of rows as the "
            "training set." << endl;
      }
    }

    Timer::Start("regression");
    lr = LinearRegression(regressors, responses);
    Timer::Stop("regression");

    // Save the parameters.
    if (CLI::HasParam("output_model"))
      CLI::GetParam<LinearRegression>("output_model") = std::move(lr);
  }

  // Did we want to predict, too?
  if (CLI::HasParam("test"))
  {
    // A model file was passed in, so load it.
    if (!computeModel)
    {
      Timer::Start("load_model");
      lr = std::move(CLI::GetParam<LinearRegression>("input_model"));
      Timer::Stop("load_model");
    }

    // Load the test file data.
    Timer::Start("load_test_points");
    mat points = std::move(CLI::GetParam<mat>("test"));
    Timer::Stop("load_test_points");

    // Ensure that test file data has the right number of features.
    if ((lr.Parameters().n_elem - 1) != points.n_rows)
    {
      Log::Fatal << "The model was trained on " << lr.Parameters().n_elem - 1
          << "-dimensional data, but the test points in '"
          << CLI::GetPrintableParam<mat>("test") << "' are " << points.n_rows
          << "-dimensional!" << endl;
    }

    // Perform the predictions using our model.
    rowvec predictions;
    Timer::Start("prediction");
    lr.Predict(points, predictions);
    Timer::Stop("prediction");

    // Save predictions.
    if (CLI::HasParam("output_predictions"))
      CLI::GetParam<vec>("output_predictions") = std::move(predictions);
  }
}
