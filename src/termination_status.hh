#pragma once

enum class TerminationStatus {
  completed,
  datarace_exception,
  unlock_exception,
  assertion_failure_exception,
  unassigned_variable_read_exception,
};
