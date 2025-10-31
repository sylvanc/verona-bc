#pragma once

#include "lang.h"

#include <git2.h>
#include <openssl/evp.h>

namespace vc
{
  struct SHA256
  {
    EVP_MD_CTX* ctx;

    SHA256()
    {
      ctx = EVP_MD_CTX_new();
      EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    }

    ~SHA256()
    {
      EVP_MD_CTX_free(ctx);
    }

    SHA256& operator<<(const std::string_view& v)
    {
      EVP_DigestUpdate(ctx, v.data(), v.size());
      return *this;
    }

    SHA256& operator<<(Node& node)
    {
      return *this << node->location().view();
    }

    std::string str()
    {
      uint8_t hash[EVP_MAX_MD_SIZE];
      uint32_t len = 0;

      if (!EVP_DigestFinal_ex(ctx, hash, &len))
        return {};

      std::stringstream ss;
      ss << "_";

      for (uint32_t i = 0; i < len; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << int(hash[i]);

      return ss.str();
    }
  };

  struct Dependency
  {
    git_repository* repo = nullptr;
    git_object* obj = nullptr;
    git_commit* commit = nullptr;
    git_remote* remote = nullptr;

    Node url;
    Node tag;
    Node dir;

    std::string hash;
    std::filesystem::path repo_path;
    std::filesystem::path src_path;

    Dependency(Node url, Node tag, Node dir) : url(url), tag(tag), dir(dir)
    {
      hash = (SHA256() << url << tag).str();
      repo_path = std::filesystem::path("_vdeps") / hash;
      src_path = repo_path;

      if (dir)
        src_path /= dir->location().view();
    }

    ~Dependency()
    {
      git_remote_free(remote);
      git_commit_free(commit);
      git_object_free(obj);
      git_repository_free(repo);
    }

    bool fetch()
    {
      // Make a consistent path to this repo.
      auto str_url = std::string(url->location().view());
      auto str_tag = std::string(tag->location().view());

      // Try to open it.
      if (git_repository_open(&repo, repo_path.c_str()) == 0)
      {
        // Lookup remote 'origin'.
        if (git_remote_lookup(&remote, repo, "origin") != 0)
        {
          if (git_remote_create(&remote, repo, "origin", str_url.c_str()) != 0)
            return git_err(url, "Failed to open remote 'origin'");
        }

        // Fetch the latest changes.
        git_fetch_options fetch_opts;
        git_fetch_options_init(&fetch_opts, GIT_FETCH_OPTIONS_VERSION);
        fetch_opts.download_tags = GIT_REMOTE_DOWNLOAD_TAGS_ALL;
        fetch_opts.prune = GIT_FETCH_PRUNE;

        if (git_remote_fetch(remote, nullptr, &fetch_opts, nullptr) != 0)
          return git_err(url, "Failed to fetch from remote 'origin'");

        auto branch = std::format("{}/{}", git_remote_name(remote), str_tag);
        git_reference* ref = nullptr;

        if (
          git_branch_lookup(&ref, repo, branch.c_str(), GIT_BRANCH_REMOTE) == 0)
        {
          const git_oid* oid = git_reference_target(ref);

          if (oid != nullptr)
            git_commit_lookup(&commit, repo, oid);

          git_reference_free(ref);
        }
      }
      else
      {
        // If we can't open it, clone it.
        git_clone_options clone_opts;
        git_checkout_options checkout_opts;
        git_clone_options_init(&clone_opts, GIT_CLONE_OPTIONS_VERSION);
        git_checkout_options_init(&checkout_opts, GIT_CHECKOUT_OPTIONS_VERSION);

        // We'll do the checkout manually after resolving the rev
        checkout_opts.checkout_strategy = GIT_CHECKOUT_NONE;
        clone_opts.checkout_opts = checkout_opts;

        if (
          git_clone(&repo, str_url.c_str(), repo_path.c_str(), &clone_opts) !=
          0)
          return git_err(url, "Failed to clone dependency");
      }

      // If it wasn't a remote branch, try a DWIM ref.
      if (commit == nullptr)
      {
        git_reference* ref = nullptr;

        if (git_reference_dwim(&ref, repo, str_tag.c_str()) == 0)
        {
          git_reference* resolved = nullptr;

          if (git_reference_resolve(&resolved, ref) == 0)
          {
            const git_oid* oid = git_reference_target(resolved);

            if (oid != nullptr)
              git_commit_lookup(&commit, repo, oid);

            git_reference_free(resolved);
          }

          git_reference_free(ref);
        }
      }

      // If we couldn't resolve, try as a direct commit hash.
      if (commit == nullptr)
      {
        if (git_revparse_single(&obj, repo, str_tag.c_str()) != 0)
          return git_err(tag, "Failed to resolve dependency revision");

        // Peel to a commit (tags can point to tag objects).
        git_object* peeled = nullptr;

        if (git_object_peel(&peeled, obj, GIT_OBJECT_COMMIT) != 0)
          return git_err(tag, "Failed to peel to commit");

        commit = (git_commit*)peeled;
      }

      // Checkout the commit tree.
      git_checkout_options co;
      git_checkout_options_init(&co, GIT_CHECKOUT_OPTIONS_VERSION);
      co.checkout_strategy = GIT_CHECKOUT_SAFE;

      if (git_checkout_tree(repo, (git_object*)commit, &co) != 0)
        return git_err(tag, "Failed to checkout tree");

      // Detached HEAD at specific commit.
      if (git_repository_set_head_detached(repo, git_commit_id(commit)) != 0)
        return git_err(tag, "Failed to set head detached");

      if (!std::filesystem::is_directory(src_path))
      {
        url->parent()->replace(url, err(url, "Dependency is not a directory"));
        return false;
      }

      return true;
    }

    bool git_err(Node where, std::string_view msg)
    {
      auto e = git_error_last();
      auto node = err(
        where,
        std::format(
          "{}: {}", msg, (e && e->message) ? e->message : "no detailed info"));

      where->parent()->replace(where, node);
      return false;
    }
  };
}
