"""Seed the database with initial team members from markdown files"""
import psycopg2
from psycopg2 import OperationalError

# Sample data matching the current team structure
INITIAL_TEAM_MEMBERS = [
    {
        "name": "Dr. Kenneth Hayworth",
        "title": "Board Member",
        "department": "Board Members",
        "description": "President and Co-Founder of the Brain Preservation Foundation",
        "sort_order": 1
    },
    {
        "name": "Gabriel Johansson",
        "title": "Secretary",
        "department": "Board Members",
        "description": "Works as a strategist within the public sector",
        "sort_order": 2
    },
    {
        "name": "Dr. Randal Koene",
        "title": "President and Chief Science Officer",
        "department": "Board Members",
        "description": "Neuroscientist and co-founder of carboncopies.org",
        "sort_order": 3
    },
    {
        "name": "Thomas Liao",
        "title": "Director of BrainGenix",
        "department": "Board Members",
        "description": "",
        "sort_order": 4
    },
    {
        "name": "Alexander McLin",
        "title": "Chair of the Board",
        "department": "Board Members",
        "description": "",
        "sort_order": 5
    },
    {
        "name": "Stefania Schino",
        "title": "Chief Financial Officer",
        "department": "Board Members",
        "description": "Received her bachelor's degree in Accounting and Finance from Bari University of Economy",
        "sort_order": 6
    },
    {
        "name": "Alicia Smallwood",
        "title": "Board Member",
        "department": "Board Members",
        "description": "Currently studying mathematics at University of Wisconsin - Milwaukee",
        "sort_order": 7
    },
    {
        "name": "Dr. Keith Wiley",
        "title": "Board Member",
        "department": "Board Members",
        "description": "Received his Ph.D. in Computer Science and Bachelor's in Psychology",
        "sort_order": 8
    },
    # Team Members
    {
        "name": "Jason Wong",
        "title": "Director of HR/Talent Acquisition, Director of Communications",
        "department": "Team Members",
        "description": "Third year undergraduate student at UC Santa Cruz studying Economics",
        "sort_order": 10
    },
    {
        "name": "Dr. Michael Cerullo",
        "title": "",
        "department": "Team Members",
        "description": "Michael A. Cerullo, M.D. graduated from the Saint Louis University School of Medicine in 2000",
        "sort_order": 11
    },
    {
        "name": "Keiland Cooper",
        "title": "Researcher",
        "department": "Team Members",
        "description": "Neuroscientist and artificial intelligence researcher",
        "sort_order": 12
    },
    {
        "name": "Bill Croughan",
        "title": "Researcher",
        "department": "Team Members",
        "description": "Bill double-majored in math and computer science at Carnegie Mellon University",
        "sort_order": 13
    },
    {
        "name": "Matthew T. Dearing",
        "title": "",
        "department": "Team Members",
        "description": "AI/ML applications developer and Ph.D. student in computer science at Illinois Institute of Technology",
        "sort_order": 14
    },
    # Advisors
    {
        "name": "Prof. Theodore Berger",
        "title": "",
        "department": "Advisors",
        "description": "",
        "sort_order": 20
    },
    {
        "name": "Dr. Diana Deca",
        "title": "",
        "department": "Advisors",
        "description": "",
        "sort_order": 21
    },
    {
        "name": "Dr. Ben Goertzel",
        "title": "",
        "department": "Advisors",
        "description": "",
        "sort_order": 22
    },
    {
        "name": "Todd Huffman",
        "title": "",
        "department": "Advisors",
        "description": "",
        "sort_order": 23
    },
    {
        "name": "James Ingallinera",
        "title": "",
        "department": "Advisors",
        "description": "",
        "sort_order": 24
    },
    {
        "name": "Dr. Eugen Leitl",
        "title": "",
        "department": "Advisors",
        "description": "",
        "sort_order": 25
    },
    {
        "name": "Prof. Anthony Zador",
        "title": "",
        "department": "Advisors",
        "description": "",
        "sort_order": 26
    },
]


def seed_database():
    """Seed the database with initial data"""
    try:
        conn = psycopg2.connect(
            dbname="ccf_website",
            user="chardapple",
            host="localhost",
            port="5432"
        )
        
        with conn.cursor() as cur:
            # Check if data already exists
            cur.execute("SELECT COUNT(*) FROM team_members")
            count = cur.fetchone()[0]
            
            if count > 0:
                print(f"Database already has {count} team members. Skipping seed.")
                return
            
            # Insert initial data
            for member in INITIAL_TEAM_MEMBERS:
                cur.execute(
                    """INSERT INTO team_members 
                       (name, title, department, description, sort_order)
                       VALUES (%s, %s, %s, %s, %s)""",
                    (
                        member["name"],
                        member["title"],
                        member["department"],
                        member["description"],
                        member["sort_order"]
                    )
                )
            
            conn.commit()
            print(f"Successfully seeded {len(INITIAL_TEAM_MEMBERS)} team members")
        
    except OperationalError as e:
        print(f"Error seeding database: {e}")
        print("Please ensure PostgreSQL is running and database 'ccf_website' exists")
    finally:
        if 'conn' in locals():
            conn.close()


if __name__ == "__main__":
    seed_database()
