#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>
#include <regex>
#include <map>
#include <chrono>
#include <algorithm>

/*
    How to use rearchive.exe:
	    make a copy of procts
		run navigate to the copy of procts directory
        Run one of the two:
        - rearchive.exe usb
		- rearchive.exe github
		To create an empty archive use one of the two:
        - init_usb.sh
        - init_github.sh
        To delete the git archives use:
        - delete_git.sh
        When switching from another archive type edit:
        - share000/intf/repo/repo.h line #define REPO_ARCHIVE_TYPE
        To fill the archive use one of the two:
        - rearchive_usb.sh
        - rearchive_github.sh
        To delete the remote github archive use:
        - delete_github.sh
*/

struct repository_t
{
    std::string m_local;
	std::string m_remote;
};

void parse_flying_start(std::filesystem::path const& flying_start_dir, std::vector<repository_t>& repositories)
{
    std::filesystem::path flying_start_path = flying_start_dir / "flying_start.cpp";
    std::ifstream flying_start(flying_start_path);
    if (!flying_start.is_open())
    {
        std::stringstream ss;
        ss << "'" << flying_start_path << "' cannot be opened for reading" << std::endl;
        std::runtime_error(ss.str());
    }
    std::string line;
    while (std::getline(flying_start, line))
    {
        std::istringstream iss(line);
        char sep[6] = { '/0' };
        repository_t repository;
        iss >> sep[0] >> std::quoted(repository.m_local    ) >> sep[1]
                      >> std::quoted(repository.m_remote   ) >> sep[2];
        if (sep[0] == '{' && sep[1] == ',' && sep[2] == ',' )
        {
            repositories.push_back(repository);
        }
    }
}

struct submodule_t
{
    int m_linenr;
    std::string m_path;
    std::string m_url;
};

std::ostream& operator<<(std::ostream& os, submodule_t const& submodule)
{
    return os 
        << "\tpath = " << submodule.m_path << std::endl
        << "\turl = " << submodule.m_url;
}

void parse_submodules(std::filesystem::path const& stem, std::vector<submodule_t>& submodules)
{
    std::filesystem::path gitmodules_path = stem / ".gitmodules";
    std::ifstream gitmodules(gitmodules_path);
    if (!gitmodules.is_open())
    {
        std::stringstream ss;
        ss << "'" << gitmodules_path << "' cannot be opened for reading" << std::endl;
        std::runtime_error(ss.str());
    }
    int linenr = 0;
    std::string line;
    submodule_t submodule;
    while (std::getline(gitmodules, line))
    {
        ++linenr;
        std::istringstream iss(line);
        char sep = '/0';
        std::string name, value;
        iss >> name >> sep >> value;
        if (name == "path" && sep == '=')
        {
            submodule.m_linenr = linenr;
            submodule.m_path = value;
        }
        else if(name == "url" && sep == '=')
        {
            submodule.m_url = value;
            submodules.push_back(submodule);
        }
    }
}

struct stem_t
{
	explicit stem_t(std::filesystem::path const& path)
		: m_path(path)
	{}
	std::filesystem::path m_path;
	std::vector<repository_t> m_repositories;
    std::vector<submodule_t> m_submodules;
};

std::vector<stem_t> collect_data(std::filesystem::path procts_dir)
{
	std::vector<stem_t> stems;
	for (std::filesystem::directory_entry const& stem : std::filesystem::directory_iterator(procts_dir))
	{
		std::filesystem::path flying_start_dir = stem.path() / "comp" / "flying_start";
		if (std::filesystem::exists(flying_start_dir))
		{
			stems.emplace_back(stem.path());
            parse_flying_start(flying_start_dir, stems.back().m_repositories);
            parse_submodules(stem.path(), stems.back().m_submodules);
        }
	}
	return stems;
}

void convert_data(std::vector<stem_t>& stems)
{}

void create_init_usb_script(std::vector<stem_t> const& stems)
{
    std::ofstream script("init_usb.sh");
    script << "cd ../procts_repo" << std::endl;
    script << "mkdir git" << std::endl;
    script << "cd git" << std::endl;
    for (stem_t const& stem : stems)
    {
        script << "# stem: " << stem.m_repositories.back().m_local << std::endl;
        std::string guid = stem.m_repositories.back().m_remote;
        script
            << "git init --bare " 
            << guid 
            << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            script << "# submodule: " << submodule.m_path << std::endl;
            std::string guid = ((submodule.m_url.back() == '/') || (submodule.m_url.back() == '\\')) ?
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(33)), 32) :
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(32)));
            script
                << "git init --bare " 
                << guid 
                << std::endl;
        }
    }
}

void create_init_github_script(std::vector<stem_t> const& stems, char const* user, char const* githubapiurl)
{
    std::ofstream script("init_github.sh");
    script << "echo -n Password:" << std::endl;
    script << "read -s password" << std::endl;
    for (stem_t const& stem : stems)
    {
        script << "# stem: " << stem.m_repositories.back().m_local << std::endl;
        std::string guid = stem.m_repositories.back().m_remote;
        script
            << "curl -u "
            << user
            << ":$password "
            << githubapiurl
            << "/user/repos -d '{\"name\":\""
            << guid
            << "\"}'"
            << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            script << "# submodule: " << submodule.m_path << std::endl;
            std::string guid = ((submodule.m_url.back() == '/') || (submodule.m_url.back() == '\\')) ?
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(33)), 32) :
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(32)));
            script
                << "curl -u "
                << user
                << ":$password "
                << githubapiurl
                << "/user/repos -d '{\"name\":\""
                << guid
                << "\"}'"
                << std::endl;
        }
    }
}

void create_delete_github_script(std::vector<stem_t> const& stems, char const* user, char const* githubapiurl)
{
    std::ofstream script("delete_github.sh");
/*    script << "echo -n Password:" << std::endl;
    script << "read -s password" << std::endl;
    script
        << "curl -v -u "
        << user
        << ":$password -X POST "
        << githubapiurl
        << "/authorizations -d '{\"scopes\":[\"delete_repo\"], \"note\":\"token with delete repo scope\"}'"
        << std::endl;
    script << "echo -n Token:" << std::endl;
    script << "read token" << std::endl; */
    script << "export token=f214e417140883c021fa597174e9d8b6880fc255" << std::endl;

    for (stem_t const& stem : stems)
    {
        script << "# stem: " << stem.m_repositories.back().m_local << std::endl;
        std::string guid = stem.m_repositories.back().m_remote;
        script
            << "curl -X DELETE -H \"Authorization: token $token\" "
            << githubapiurl
            << "/repos/"
            << user
            << "/"
            << guid
            << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            script << "# submodule: " << submodule.m_path << std::endl;
            std::string guid = ((submodule.m_url.back() == '/') || (submodule.m_url.back() == '\\')) ?
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(33)), 32) :
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(32)));
            script
                << "curl -X DELETE -H \"Authorization: token $token\" "
                << githubapiurl
                << "/repos/"
                << user
                << "/"
                << guid
                << std::endl;
        }
    }
}

void create_delete_git_script(std::vector<stem_t> const& stems)
{
    std::ofstream script("delete_git.sh");
    for (stem_t const& stem : stems)
    {
        script << "rm -rf " << stem.m_repositories.back().m_local << "/.git" << std::endl;
        script << "rm -f " << stem.m_repositories.back().m_local << "/.gitmodules" << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            script << "rm -f " << stem.m_repositories.back().m_local << "/" << submodule.m_path << "/.git" << std::endl;
        }
    }
}

void create_rearchive_usb_script(
    std::vector<stem_t> const& stems, 
    char const* url1,
    char const* url2)
{
    std::ofstream script("rearchive_usb.sh");
    script << "hier moet nog iets gebeuren m.b.t. relative paths .." << std::endl;
    script << "export PROCTSDIR=$(pwd)" << std::endl;
    for (stem_t const& stem : stems)
    {
        std::string guid = stem.m_repositories.back().m_remote;
        script << "cd " << stem.m_repositories.back().m_local << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            std::string guid = ((submodule.m_url.back() == '/') || (submodule.m_url.back() == '\\')) ?
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(33)), 32) :
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(32)));
            std::string subm = submodule.m_path;
            script << "cd " << subm << std::endl;
            script << "git init" << std::endl;
            script << "git add --all" << std::endl;
            script << "git commit -m \"Initial check-in\"" << std::endl;
            script << "git remote add origin " << url1 << url2 << guid << "/" << std::endl;
            script << "git push origin master" << std::endl;
            script << "cd $PROCTSDIR" << std::endl;
            script << "cd " << stem.m_repositories.back().m_local << std::endl;
            script << "rm -rf " << subm  << std::endl;
        }
        script << "git init" << std::endl;
        script << "git submodule init" << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            std::string guid = ((submodule.m_url.back() == '/') || (submodule.m_url.back() == '\\')) ?
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(33)), 32) :
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(32)));
            std::string subm = submodule.m_path;
            script << "git submodule add " << url1 << url2 << guid << "/ " << subm << std::endl;
        }
        script << "git add --all" << std::endl;
        script << "git commit -m \"Initial check-in\"" << std::endl;
        script << "git remote add origin " << url1 << url2 << guid << "/" << std::endl;
        script << "git push origin master" << std::endl;
        script << "cd $PROCTSDIR" << std::endl;
    }
}

void create_rearchive_github_script(
    std::vector<stem_t> const& stems,
    char const* user,
    char const* url1,
    char const* url2)
{
    std::ofstream script("rearchive_github.sh");
    script << "export PROCTSDIR=$(pwd)" << std::endl;
    script << "echo -n Password:" << std::endl;
    script << "read -s password" << std::endl;
    for (stem_t const& stem : stems)
    {
        std::string guid = stem.m_repositories.back().m_remote;
        script << "cd " << stem.m_repositories.back().m_local << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            std::string guid = ((submodule.m_url.back() == '/') || (submodule.m_url.back() == '\\')) ?
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(33)), 32) :
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(32)));
            std::string subm = submodule.m_path;
            script << "cd " << subm << std::endl;
            script << "git init" << std::endl;
            script << "git add --all" << std::endl;
            script << "git commit -m \"Initial check-in\"" << std::endl;
            script 
                << "git push "
                << url1
                << user
                << ":$password@"
                << url2 
                << '/'
                << user 
                << '/'
                << guid
                << " master"
                << std::endl;
            script << "cd $PROCTSDIR" << std::endl;
            script << "cd " << stem.m_repositories.back().m_local << std::endl;
            script << "rm -rf " << subm << std::endl;
        }
        script << "git init" << std::endl;
        script << "git submodule init" << std::endl;
        for (submodule_t const& submodule : stem.m_submodules)
        {
            std::string guid = ((submodule.m_url.back() == '/') || (submodule.m_url.back() == '\\')) ?
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(33)), 32) :
                submodule.m_url.substr(std::max(size_t(0), submodule.m_url.size() - size_t(32)));
            std::string subm = submodule.m_path;
            script << "git submodule add " << url1 << url2 << '/' << user << '/' << guid << "/ " << subm << std::endl;
        }
        script << "git add --all" << std::endl;
        script << "git commit -m \"Initial check-in\"" << std::endl;
        script << "git remote add origin " << url1 << url2 << '/' << user << '/' << guid << "/" << std::endl;
        script
            << "git push "
            << url1
            << user
            << ":$password@"
            << url2
            << '/'
            << user
            << '/'
            << guid
            << " master"
            << std::endl;
        script << "cd $PROCTSDIR" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    try
    {
        bool usb = false;
        bool github = false;
        if (argc > 1)
        {
            if (!strcmp(argv[1], "usb"))
            {
                usb = true;
            }
            else if (!strcmp(argv[1], "github"))
            {
                github = true;
            }
        }
        if (!usb && !github)
        {
            throw std::runtime_error("Expected arguement usb or github");
        }

        std::vector<stem_t> stems = collect_data(std::filesystem::current_path());
        convert_data(stems);
        if (github)
        {
            char const* user = "kubicas";
            char const* githubapiurl = "https://api.github.com";
            char const* url1 = "https://";
            char const* url2 = "github.com";
            create_init_github_script(stems, user, githubapiurl);
            create_delete_github_script(stems, user, githubapiurl);
            create_rearchive_github_script(stems, user, url1, url2); // "https://github.com/kubicas/c623d50ade32453f9c5c1808d47ade11.git"
        }
        if(usb)
        {
            char const* url1 = "file://../../";
            char const* url2 = "procts_repo/git/";
            create_init_usb_script(stems);
            create_rearchive_usb_script(stems, url1, url2); // "file://../../<<../>>procts_repo/git/"
        }
        create_delete_git_script(stems);
        std::cout << "Ready" << std::endl;
    }
    catch (std::exception const& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
	return 0;
}

