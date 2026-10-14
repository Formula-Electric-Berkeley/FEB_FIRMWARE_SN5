/**
 * @file main.cpp
 * @brief bms_sim command line.
 *
 *   bms_sim list                      list scenarios
 *   bms_sim run <name> [--csv FILE]   run one scenario with full log output
 *   bms_sim all                       run every scenario (one process each)
 *
 *   -v  debug logging     -q  errors only
 */
#include "scenario.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

void usage()
{
  std::printf("usage: bms_sim list\n"
              "       bms_sim run <scenario> [--csv FILE] [-v|-q]\n"
              "       bms_sim all [-v|-q]\n");
}

const sim::Scenario *find(const char *name)
{
  for (const sim::Scenario &sc : sim::scenarios())
  {
    if (std::strcmp(sc.name, name) == 0)
    {
      return &sc;
    }
  }
  return nullptr;
}

int run_one(const sim::Scenario &sc, const char *csv_path)
{
  std::printf("=== %s: %s\n", sc.name, sc.summary);

  sim::Sim s;
  std::FILE *csv = nullptr;
  if (csv_path)
  {
    csv = std::fopen(csv_path, "w");
    if (!csv)
    {
      std::perror(csv_path);
      return 2;
    }
    s.set_trace(csv);
  }

  sim::Check c;
  sc.run(s, c);

  if (csv)
  {
    std::fclose(csv);
  }

  std::printf("--- state history:");
  for (const sim::StateChange &h : sim::state_history())
  {
    std::printf(" %s@%u", sim::state_name(h.state), h.t_ms);
  }
  std::printf("\n%s %s\n", c.passed() ? "PASS" : "FAIL", sc.name);
  return c.passed() ? 0 : 1;
}

int run_all()
{
  int failed = 0;
  for (const sim::Scenario &sc : sim::scenarios())
  {
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
      int rc = run_one(sc, nullptr);
      std::fflush(stdout);
      std::_Exit(rc);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
      failed++;
    }
    std::printf("\n");
  }
  std::printf("%zu scenarios, %d failed\n", sim::scenarios().size(), failed);
  return failed ? 1 : 0;
}

} // namespace

int main(int argc, char **argv)
{
  const char *csv_path = nullptr;
  std::vector<const char *> args;
  for (int i = 1; i < argc; i++)
  {
    if (std::strcmp(argv[i], "-v") == 0)
    {
      sim::set_log_level(FEB_LOG_DEBUG);
    }
    else if (std::strcmp(argv[i], "-q") == 0)
    {
      sim::set_log_level(FEB_LOG_ERROR);
    }
    else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc)
    {
      csv_path = argv[++i];
    }
    else
    {
      args.push_back(argv[i]);
    }
  }

  if (args.empty())
  {
    usage();
    return 2;
  }

  std::string cmd = args[0];
  if (cmd == "list")
  {
    for (const sim::Scenario &sc : sim::scenarios())
    {
      std::printf("%-40s %s\n", sc.name, sc.summary);
    }
    return 0;
  }
  if (cmd == "all")
  {
    return run_all();
  }
  if (cmd == "run" && args.size() == 2)
  {
    const sim::Scenario *sc = find(args[1]);
    if (!sc)
    {
      std::fprintf(stderr, "unknown scenario '%s' (try: bms_sim list)\n", args[1]);
      return 2;
    }
    return run_one(*sc, csv_path);
  }

  usage();
  return 2;
}
