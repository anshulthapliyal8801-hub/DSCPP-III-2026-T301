# MUNSphere demonstration

A complete walkthrough of the system on the demo conference, in five acts,
about 20 minutes. Every step uses the real application; nothing is mocked.

## Before you start

```powershell
Remove-Item -Recurse -Force demo-data -ErrorAction SilentlyContinue   # fresh demo
.\run.bat demo
```

Open **three browser windows**, one per role: a normal window, a private window
and a second browser (each keeps its own login). Log in at
`http://127.0.0.1:8080/login.html` on the right tab:

| Window | Tab | ID | Password |
|---|---|---|---|
| 1 | Admin | `ADM001` | `Admin2026` |
| 2 | Executive Board | `EB001` (UNGA Chairperson) | `Board2026` |
| 3 | Delegate | `D001` (India, UNGA) | `Delegate2026` |

What the demo data contains: *Graphic Era MUN 2026* with UNGA, UNSC and AIPPM;
24 delegates (22 placed, `D023` waiting for review, `D024` rejected); seven EB
members; UNGA's opening session finished and its **general debate live now**;
UNGA results published, UNSC marksheets waiting for review (one returned),
AIPPM marksheets in draft; three announcements; one backup.

---

## Act 1: The conference team (Admin, window 1)

1. **Overview**: 3 committees, 22 placed, *1 registration to review*, UNGA's
   placard *In session*. The landing page (`/`) shows the same placard publicly.
2. **Delegates**: the list is alphabetical (an in-order walk of the delegate
   BST). Filter *Waiting for review*: approve **Karan Mehra (D023)**, then
   **Place** him in UNSC as *India*. Try *United States* first: refused, the
   portfolio is taken. UNSC has room; UNGA (capacity 12, 8 placed) would too.
3. **Executive Board**: seven members, each a Chairperson, Moderator or Scribe of
   one committee. *Edit* the UNGA Scribe (`EB003`) to see role permissions as
   checkboxes. *New EB member* shows a temporary password exactly once.
4. **Committees → Roster** for UNSC: sorted by insertion sort, with its
   comparison and move counts.

## Act 2: A live committee (windows 2 and 3)

5. **Chair (window 2) → Live session**: Japan has the floor and is already over
   time (the clock is red; it turns brass at 10 seconds left). Germany, Mexico
   and Brazil are waiting; Egypt has asked to speak; India has raised a POI.
6. **Delegate (window 3) → Live session**: India sees the same floor and clock
   (both follow the server's time) and its own POI as *Pending*.
7. Chair: **Accept** India's POI. Within 3 seconds India's page shows *Accepted*.
   Chair: **Add Egypt to the list**, then **Next speaker**: Germany takes the
   floor and Japan's speech joins the history.
8. Delegate: **Request to speak**, then see your place appear once the chair
   approves it. Raise another POI to Germany.
9. **Motions**: Kenya's *Unmoderated caucus* is listed above Indonesia's
   *Set the speaking time*: precedence comes from a binary heap, not arrival
   order. Chair: **Put unmoderated caucus to a vote**. Delegate: it's procedural,
   so only *Yes* and *No*. Vote *No*.
10. Chair: the tally updates as delegates vote. **Close vote and count**. Then put
    the *speaking time* motion to a vote; when it passes, the **floor clock
    changes to 60 s**: a passed motion takes effect.
11. Chair: rule a motion *out of order*, then **Undo** (a stack of chair
    actions). Try undo after ballots exist: refused, with the reason.
12. **Attendance** (chair): every delegate *Present*; switching one to *Late* or
    *Absent* updates the sheet without creating a duplicate.

## Act 3: Results (all three windows)

13. **Admin → Marksheets**: UNSC's seven are *Waiting for review*. **View** one:
    the criteria, the EB's remarks and the full revision history. *Approve* two.
    One UNSC sheet was *Returned* with a note.
14. **Chair → Evaluation** shows UNGA's eight published sheets and the ranking.
    (Log in as `EB004`, UNSC's chair, to see the returned sheet highlighted with
    the Admin's note, correct it and resubmit.)
15. **Admin → Rankings**, UNGA: India first with *Best Delegate*. Ties share a
    rank; the right-hand column says what separated each delegate from the one
    above (score, tie-break criterion, attendance). Switch the sort algorithm:
    the ranking stays the same, the measured work changes.
16. **Delegate → Results**: India's award placard, 87%, rank 1 of 8, the
    marksheet bars and the chair's remarks. No internal notes are shown.
17. **Admin → Scoring & awards**: criteria from the specification (total 100)
    and the award thresholds, both editable. New marksheets use new criteria;
    existing ones keep the criteria they were marked against.

## Act 4: The data structures (Admin)

18. **Module 2 lab** (sidebar footer): the delegate hash table's buckets, with
    collisions highlighted, the load factor and resizes; the BST drawn from the
    real tree with its three traversals; all four searches on one key.
19. **DSA analytics**: every delegate looked up by hash table, linear search,
    binary search and BST, with average and worst comparisons; *Show the path* for
    `D012` traces all four. Structure shapes: load factor, BST height against
    the minimum possible.
20. **Rankings → Run benchmark** with 2,000 entries, *Already sorted*: bubble and
    insertion sort make n−1 comparisons, selection and quick sort n(n−1)/2
    (quick sort's worst case with a last-element pivot), merge sort far fewer.
    Switch to *Random* and compare.
21. **Module 1 lab**: the conference, committee and session linked lists, with a
    traced linear search.

## Act 5: Security and data (Admin)

22. In the delegate window, open `http://127.0.0.1:8080/admin.html`: sent back to
    the delegate dashboard. The server refuses the API calls too: the
    authorization test checks all 130 endpoints against six identities.
23. **Executive Board → Revoke** `EB001`. The chair's window is signed out at its
    next refresh and can't log back in. **Restore** it.
24. **Data & audit**: the audit log lists every change in this demo, with who and
    from which role, including refused attempts. *Verify hash chain*: intact.
25. **Back up now**; then delete an announcement; then **Restore** the backup: the
    announcement is back, a safety backup was kept, and the audit log still
    records the deletion and the restore (it is never rolled back).
26. **Check consistency**: save → reload → save gives identical files for all 22
    stores, and no broken links between them.
27. Tamper test (optional): stop the server, change one word in
    `demo-data\audit_log.txt` with Notepad, start again, *Verify hash chain*: it
    reports the exact entry that no longer matches.
28. **Reports**: *Scoring* with mean, median, standard deviation and histogram;
    **Export CSV** and open it in Excel.

## Afterwards

Stop with Ctrl+C. Delete `demo-data\` to reset. Real conference data is in
`data\` and is never touched by the demo.
