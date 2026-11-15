import { useState } from "react";
import { send, useNative, type RecentTitle } from "../native";
import { whenPlayed } from "../content/home";
import type { BladeTone } from "./blade";
import { playSound } from "./sound";
import { closeSubBlade, openSubBlade, SubBlade } from "./sub-blade";
import { showToast } from "./toast";
import { t, type MessageKey } from "../i18n";

/**
 * All Games: every game in the game folders and every game played on this
 * Mac, on a sub-blade from the left, as the 360's game library was. A list
 * (each row the game's icon, its name, and its achievements and gamerscore
 * when a signed-in profile has them) or a grid of cards with the same on
 * each. A plays, B goes back, X changes the order, Y changes the view.
 */

export const openAllGames = () => openSubBlade("all-games");

/** X steps through these in this order: the two orders, then each backwards. */
const SORTS = ["recent", "alpha", "leastRecent", "alphaReverse"] as const;
type Sort = (typeof SORTS)[number];
const SORT_KEY = "macxenos.allGames.sort";
const SORT_LABEL: Record<Sort, MessageKey> = {
  recent: "allGames.sort.recent",
  leastRecent: "allGames.sort.leastRecent",
  alpha: "allGames.sort.alpha",
  alphaReverse: "allGames.sort.alphaReverse"
};

type View = "list" | "cards";
const VIEW_KEY = "macxenos.allGames.view";
const VIEW_LABEL: Record<View, MessageKey> = { list: "allGames.view.list", cards: "allGames.view.cards" };

function saved<T extends string>(key: string, values: readonly T[], fallback: T): T {
  try {
    const value = window.localStorage.getItem(key);
    return values.includes(value as T) ? (value as T) : fallback;
  } catch {
    return fallback;
  }
}

function remember(key: string, value: string) {
  try {
    window.localStorage.setItem(key, value);
  } catch {
    // Remembering the order and the view is a convenience.
  }
}

const titleOf = (game: RecentTitle) => {
  const name = game.name || game.path.split("/").pop() || game.path;
  return game.disc ? t("allGames.disc", { name, disc: game.disc }) : name;
};

const byName = (a: RecentTitle, b: RecentTitle) =>
  titleOf(a).localeCompare(titleOf(b), undefined, { numeric: true, sensitivity: "base" });

/** Most recently played: played games newest first, then the rest A to Z.
    Each of the other two orders' partner is that order backwards. */
export function sorted(games: RecentTitle[], sort: Sort): RecentTitle[] {
  const alpha = sort === "alpha" || sort === "alphaReverse";
  const list = [...games].sort(alpha ? byName : (a, b) => b.lastPlayed - a.lastPlayed || byName(a, b));
  return sort === "alphaReverse" || sort === "leastRecent" ? list.reverse() : list;
}

export function AllGames({ tone }: { tone: BladeTone }) {
  const { allGames, gameFolders, profiles } = useNative();
  // The tab says whose library this is, not only who is signed in.
  const player = profiles.find((profile) => profile.slot === 0);
  const [sort, setSort] = useState<Sort>(() => saved(SORT_KEY, SORTS, "recent"));
  const [view, setView] = useState<View>(() => saved(VIEW_KEY, ["list", "cards"] as const, "list"));
  const games = sorted(allGames, sort);

  const play = (index: number) => {
    const game = games[index];
    if (!game) return;
    closeSubBlade();
    send({ op: "launch", path: game.path });
  };
  const changeSort = () => {
    if (games.length < 2) return;
    const next = SORTS[(SORTS.indexOf(sort) + 1) % SORTS.length];
    setSort(next);
    remember(SORT_KEY, next);
    playSound("toggle");
    showToast(t("allGames.title"), t(SORT_LABEL[next]), { sound: false });
  };
  const changeView = () => {
    const next: View = view === "list" ? "cards" : "list";
    setView(next);
    remember(VIEW_KEY, next);
    playSound("toggle");
  };

  const sub = (game: RecentTitle) =>
    game.achievements
      ? t("allGames.achievements", { unlocked: game.achievements.unlocked, total: game.achievements.total })
      : game.lastPlayed
        ? whenPlayed(game.lastPlayed)
        : t("played.never");
  const score = (game: RecentTitle) =>
    game.achievements ? (
      <span className="game-score inline-flex items-center gap-[0.25em]">
        {game.achievements.score}
        <span className="gamerscore-g" aria-label={t("allGames.gamerscore")}>
          G
        </span>
      </span>
    ) : null;

  return (
    <SubBlade
      id="all-games"
      side="left"
      tone={tone}
      title={t("allGames.title")}
      tabLabel={player?.gamertag ? t("allGames.tab.player", { name: player.gamertag }) : t("allGames.tab")}
      count={games.length}
      onActivate={play}
      onX={changeSort}
      onY={changeView}
      grid={view === "cards"}
      resetKey={sort}
      legend={() => ({
        left: [
          // Y names the view it changes to; X the order in force.
          ...(games.length
            ? [{ button: "y" as const, label: t(VIEW_LABEL[view === "list" ? "cards" : "list"]), onClick: changeView }]
            : []),
          ...(games.length > 1
            ? [{ button: "x" as const, label: t("allGames.sort", { order: t(SORT_LABEL[sort]) }), onClick: changeSort }]
            : [])
        ],
        right: [
          { button: "b", label: t("legend.back") },
          ...(games.length ? [{ button: "a" as const, label: t("legend.select") }] : [])
        ]
      })}
      empty={
        gameFolders.scanning
          ? t("allGames.looking")
          : gameFolders.folders.length
            ? t("allGames.noneFound")
            : t("allGames.noFolders")
      }
    >
      {(rowProps) =>
        !games.length ? null : view === "cards" ? (
          <ul
            className="game-cards m-0 grid list-none p-0"
            aria-label={`${t("allGames.title")}, ${t(SORT_LABEL[sort])}`}
          >
            {games.map((game, index) => (
              <li key={game.path} className="min-w-0">
                <button type="button" {...rowProps(index)} className="game-card relative grid w-full">
                  <img
                    className="game-card-icon"
                    src={game.icon || "/macxenos-icon-dark.webp"}
                    alt=""
                    aria-hidden="true"
                  />
                  <span className="game-card-name">{titleOf(game)}</span>
                  <span className="game-card-sub truncate">{sub(game)}</span>
                  {score(game) ?? <span className="game-card-score-gap" aria-hidden="true" />}
                </button>
              </li>
            ))}
          </ul>
        ) : (
          <ul className="subblade-list m-0 grid list-none p-0" aria-label={`${t("allGames.title")}, ${t(SORT_LABEL[sort])}`}>
            {games.map((game, index) => (
              <li key={game.path} className="subblade-item">
                <button type="button" {...rowProps(index)}>
                  <img className="game-icon" src={game.icon || "/macxenos-icon-dark.webp"} alt="" aria-hidden="true" />
                  <span className="grid min-w-0">
                    <span className="subblade-name truncate">{titleOf(game)}</span>
                    <span className="subblade-sub truncate">{sub(game)}</span>
                  </span>
                  {score(game) ?? <span />}
                </button>
              </li>
            ))}
          </ul>
        )
      }
    </SubBlade>
  );
}
